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
    });

    node.on("broadcast", [&node](const json& req) {
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
//        json topology = req["body"]["topology"];
//        // topology looks like this: {"n0" ("n3" "n1"), "n1" ("n4" "n2" "n0"), "n2" ("n1"), "n3" ("n0" "n4"), "n4" ("n1" "n3")}, set peers to this
//        for (auto& [key, value] : topology.items()) {
//            node.peers.insert(key);
//            for (auto& peer : value) {
//                node.peers.insert(peer);
//            }
//        }
//        cerr << "topology received: " << req << endl;
        node.peers = req["body"]["topology"][node.nodeId];
        json msg = {
                {"type", "topology_ok"},
        };
        node.reply(req, msg);
    });

    // start a separate thread that will periodically send gossip calls to our
    node.run();
}
