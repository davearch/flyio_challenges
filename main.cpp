#include <iostream>
#include <set>
#include <random>
#include "node.h"

int main() {
    Node node{};

    node.on("broadcast", [&node](const json& req) {
        node.reply(req, {{"type", "broadcast_ok"}});
        string msg = req["body"]["message"];
        node.peerMessages[node.nodeId].insert(msg);
        bool new_msg = false;
        if (!node.messages.contains(msg)) {
            node.messages.insert(msg);
            new_msg = true;
        }
        if (new_msg) {
            cerr << "Node " << node.nodeId << " received new message " << msg << endl;
            // make a stack of peers to send to
            set<string> peers_to_send_to;
            for (auto& peer : node.peers) {
                if (!node.peerMessages[peer].contains(msg)) {
                    peers_to_send_to.insert(peer);
                }
            }
            // send to peers
            while (!peers_to_send_to.empty()) {
                for (auto& peer : peers_to_send_to) {
                    json reply = node.rpc(peer, {{"type", "broadcast"}, {"message", msg}});
                    if (reply["type"] == "broadcast_ok") {
                        cerr << "Node " << node.nodeId << " received ack from " << peer << " for msg: " << msg << endl;
                        node.peerMessages[peer].insert(msg);
                        peers_to_send_to.erase(peer);
                    }
                }
                // wait a bit before trying again
                this_thread::sleep_for(chrono::milliseconds(100));
            }
            cerr << "Node " << node.nodeId << " finished broadcasting message " << msg << endl;
        }
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
