//
// Created by David Archuleta on 4/10/23.
//

#include "node.h"

string Node::getNodeId() {
    return this->nodeId;
}

vector<string> Node::getNodeIds() {
    return this->nodeIds;
}

int Node::newMsgId() {
    int id = this->nextMsgId;
    this->nextMsgId++;
    return id;
}

void Node::send(const string &dest, const json &body) {
    json msg = {
            {"src",  this->nodeId},
            {"dest", dest},
            {"body", body}
    };
    cerr << "Sending " << msg.dump() << endl;
    cout << msg << endl;
    cerr << "sent" << endl;
}

void Node::reply(const json &req, const json &body) {
    if (!req["body"].contains("msg_id")) {
        throw runtime_error("Cannot reply to a message without a msg_id");
    }
    json body2 = body;
    body2["in_reply_to"] = req["body"]["msg_id"];
    send(req["src"], body2);
}

json Node::rpc(const string &dest, const json &body) {
    int msgId = newMsgId();
    json body2 = body;
    body2["msg_id"] = msgId;
    promise<json> p;
    future<json> fut = p.get_future();
    replyHandlers[msgId] = [&p](const json& reply) {
        p.set_value(reply);
    };
    thread([this, dest, body2, msgId]() {
        auto it = replyHandlers.find(msgId);
        if (it != replyHandlers.end()) {
            auto handler = it->second;
            replyHandlers.erase(it);

            json err = {
                    {"type", "error"},
                    {"in_reply_to", msgId},
                    {"code", 0},
                    {"text", "RPC request timed out"}
            };
            handler(err);
        }
    }).detach();
    send(dest, body2);
    return fut.get();
}

json Node::retryRPC(const string &dest, const json &body) {
    while (true) {
        try {
            return rpc(dest, body);
        } catch (...) {
            cerr << "Retrying RPC request to " << dest << " " << body.dump() << endl;
        }
    }
}

void Node::on(const string &type, const function<void(json)> &handler) {
    handlers[type] = handler;
}

void Node::handleInit(const json &req) {
    this->nodeId = req["body"]["node_id"];
    this->nodeIds = req["body"]["node_ids"].get<vector<string>>();
    cerr << "init: " << req << endl;
    cerr << "Node " << nodeId << " initialized" << endl;
}

void Node::maybeReplyError(const json &req, const exception &e) {
    if (req["body"].contains("msg_id")) {
        json err = {
                {"type", "error"},
                {"code", 13},
                {"text", string(e.what())}
        };
        reply(req, err);
    }
}

void Node::handle(const json &req) {
    try {
        json body = req["body"];
        if (body.contains("in_reply_to")) {
            int in_reply_to = body["in_reply_to"];
            if (replyHandlers.count(in_reply_to) > 0) {
                auto handler = replyHandlers[in_reply_to];
                replyHandlers.erase(in_reply_to);
                if (body["type"] == "error") {
                    json err = {
                            {"type", "error"},
                            {"code", body["code"]},
                            {"text", body["text"]}
                    };
                    handler(err);
                } else {
                    handler(body);
                }
            }
            return;
        }

        string type = body["type"];
        if (type == "init") {
            cerr << "init called" << endl;

            handleInit(req);
            auto handler = handlers["init"];
            if (handler) {
                handler(req);
            }
            reply(req, {{"type", "init_ok"}});
            return;
        }

        if (handlers.count(type) > 0) {
            auto handler = handlers[type];
            handler(req);
        } else {
            cerr << "Don't know how to handle msg type " << type << " (" << req.dump() << ")" << endl;
            reply(req, {
                    {"type", "error"},
                    {"code", 10},
                    {"text", "unsupported request type " + type}
            });
        }
    } catch (exception& e) {
        cerr << "Error processing request " << e.what() << endl;
        maybeReplyError(req, e);
    }
}

[[noreturn]] void Node::run() {
    while (true) {
        string line;
        getline(cin, line);
        if (line.empty()) {
            continue;
        }
        json req = json::parse(line);
        thread handler_thread([this, req]() {
            this->handle(req);
        });
        handler_thread.detach();
    }
}
