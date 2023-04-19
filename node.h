//
// Created by David Archuleta on 4/10/23.
//

#ifndef FLYIO_CHALLENGES_NODE_H
#define FLYIO_CHALLENGES_NODE_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "json.hpp"
#include <set>
#include <queue>
#include <future>
#include <random>
#include "TreeNode.h"

using json = nlohmann::json;
using namespace std;

enum class InjectedPayload {
    Gossip
};

enum class Event {
    Injected,
    EndOfFile
};

struct Message {
    Event event;
    InjectedPayload payload;
};

class Node {
public:
    string nodeId;
    vector<string> nodeIds;
    int rpcTimeout = 1000;
    int nextMsgId = 0;
    map<int, function<void(json)>> replyHandlers;
    unordered_map<string, function<void(json)>> handlers;

    set<int> messages;
    set<string> peers;
    // map of peer_id -> set of messages that I know that it knows
    unordered_map<string, set<int>> peerMessages;

    Node() = default;
    string getNodeId();
    vector<string> getNodeIds();
    int newMsgId();
    void send(const string& nodeId, const json& msg);
    void reply(const json& req, const json& body);
    json rpc(const string& dest, const json& body);
    json retryRPC(const string& dest, const json& body);
    void on(const string& type, const function<void(json)>& handler);
    void handleInit(const json& req);
    void maybeReplyError(const json& req, const exception& e);
    void handle(const json& req);
    static string generate_uuid();
    void stop();
    void push_message(const Message& msg);
    void gossip();

    [[noreturn]] void run();
private:
    bool stop_gossip_thread = false;
    mutex mtx_;
    queue<Message> q_;
};


#endif //FLYIO_CHALLENGES_NODE_H



















