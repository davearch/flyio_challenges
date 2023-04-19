#include <iostream>
#include <set>
#include <random>
#include "node.h"

int main() {
    Node node{};

    node.on("echo", [&node](const json &req) {
        json msg = {
                {"type", "echo_ok"},
                {"echo", req["body"]["echo"]}
        };
        node.reply(req, msg);
    });

    node.on("generate", [&node](const json &req) {
        string uuid = Node::generate_uuid();
        json msg = {
                {"type", "generate_ok"},
                {"id", uuid}
        };
        node.reply(req, msg);
    });

    node.on("gossip", [&node](const json &req) {
        set<int> new_messages;
        for (const int msg : req["body"]["message"]) {
            new_messages.insert(msg);
        }
        node.peerMessages[req["src"]].insert(new_messages.begin(), new_messages.end());
        node.messages.insert(new_messages.begin(), new_messages.end());
        node.reply(req, {{"type", "gossip_ok"}});
    });

    node.on("gossip_ok", [&node](const json &req) {
        // add the messages they are okaying to, to their set of messages in peerMessages
        set<int> new_messages;
        for (const int msg : req["body"]["message"]) {
            new_messages.insert(msg);
        }
        node.peerMessages[req["src"]].insert(new_messages.begin(), new_messages.end());
    });

    node.on("broadcast", [&node](const json& req) {
        // print req to stderr
        cerr << "broadcast called with " << req.dump() << endl;

        int msg = req["body"]["message"];
        node.peerMessages[req["src"]].insert(msg);
        node.messages.insert(msg);
        node.reply(req, {{"type", "broadcast_ok"}});
    });

    node.on("read", [&node](const json& req) {
        json msg = {
                {"type", "read_ok"},
                {"messages", node.messages}
        };
        node.reply(req, msg);
    });

    node.on("topology", [&node](const json& req) {
        json topology = req["body"]["topology"][node.nodeId];
        for (auto& peer : topology) {
            node.peers.insert(peer);
        }
        json msg = {
                {"type", "topology_ok"},
        };
        node.reply(req, msg);
    });

    node.run();
}
