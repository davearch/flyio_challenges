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
        this_thread::sleep_for(chrono::milliseconds(rpcTimeout));

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
    thread([&]() {
        while (!stop_gossip_thread) {
            this_thread::sleep_for(chrono::milliseconds(100));
            this->push_message({Event::Injected, InjectedPayload::Gossip});
        }
    }).detach();
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

void Node::push_message(const Message& msg) {
    unique_lock<mutex> lock(mtx_);
    q_.push(msg);
}

void Node::gossip() {
    for (const auto &peer: this->peers) {
        set<int> messages_to_send;
        // for all messages that I know that the peer does not know, send them to the peer
        set_difference(this->messages.begin(), this->messages.end(),
                       this->peerMessages[peer].begin(), this->peerMessages[peer].end(),
                       inserter(messages_to_send, messages_to_send.begin()));
        // if the set_difference is empty, then skip
        if (messages_to_send.empty()) {
            continue;
        }
        // send the whole set as one message
        this->send(peer, {{"type",    "gossip"},
                         {"message", messages_to_send},
                         {"msg_id",  this->newMsgId()}});
    }
}

string Node::generate_uuid() {
    static random_device dev;
    static mt19937 rng(dev());

    uniform_int_distribution<int> dist(0, 15);

    const char *v = "0123456789abcdef";
    const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

    string res;
    for (bool i : dash) {
        if (i) res += "-";
        res += v[dist(rng)];
        res += v[dist(rng)];
    }
    return res;
}

void Node::stop() {
    stop_gossip_thread = true;
}

[[noreturn]] void Node::run() {

    while (true) {
        // launch async task to read from stdin
        auto input_future = async(launch::async, []() {
            string line;
            getline(cin, line);
            return line;
        });

        while (true) {
            if (input_future.wait_for(chrono::milliseconds(1)) == future_status::ready) {
                string line = input_future.get();
                if (!line.empty()) {
                    json req = json::parse(line);
                    handle(req);
                }
                break;
            }

            unique_lock<mutex> lock(mtx_);
            if (!q_.empty()) {
                auto msg = q_.front();
                q_.pop();
                lock.unlock();
                if (msg.event == Event::Injected) {
                    if (msg.payload == InjectedPayload::Gossip) {
                        gossip();
                    }
                } else if (msg.event == Event::EndOfFile) {
                    this->stop();
                }
            } else {
                lock.unlock();
                this_thread::yield();
            }
        }
    }
}
