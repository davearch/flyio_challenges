//
// Created by David Archuleta on 4/10/23.
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <json.hpp>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <stdexcept>

using json = nlohmann::json;

template <typename Payload>
struct Message {
    std::string src;
    std::string dst;
    Payload body;

    Message into_reply(size_t* id) {
        if (id) {
            body.id = *id;
            (*id)++;
        }
        return {dst, src, body};
    }

    void send(std::ostream& output) {
        output << json(body).dump() << std::endl;
    }
};

struct Body {
    std::optional<size_t> id;
    std::optional<size_t> in_reply_to;
    json payload;
};

enum class EventType {
    Message,
    Injected,
    EOF
};

template <typename Payload>
struct Event {
    EventType type;
    std::variant<Message<Payload>, Payload, std::monostate> data;
};

template <typename S, typename Payload>
struct Node {
    virtual ~Node() = default;

    virtual std::unique_ptr<Node> from_init(
            S state,
            const json& init,
            std::function<void(Event<Payload>)> inject
    ) = 0;

    virtual void step(
            Event<Payload> input,
            std::ostream& output
    ) = 0;
};

template <typename S, typename N, typename P>
void main_loop(S init_state) {
    std::istream &input_stream = std::cin;
    std::ostream &output_stream = std::cout;
    std::queue<Event<P>> event_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    std::string init_message_str;
    std::getline(input_stream, init_message_str);
    json init_message_json = json::parse(init_message_str);

    Message<json> init_message{
            init_message_json["src"],
            init_message_json["dest"],
            {init_message_json["msg_id"], {}, init_message_json}
    };

    std::unique_ptr<N> node = N::from_init(
            init_state,
            init_message.body.payload["init"],
            [&](Event<P> event) {
                std::unique_lock<std::mutex> lock(queue_mutex);
                event_queue.push(event);
                queue_cv.notify_one();
            }
    );

    init_message.into_reply(nullptr).send(output_stream);

    auto input_thread = std::async(std::launch::async, [&]() {
        std::string line;
        while (std::getline(input_stream, line)) {
            json message_json = json::parse(line);
            std::unique_lock<std::mutex> lock(queue_mutex);
            event_queue.push({EventType::Message,
                              {message_json["src"], message_json["dest"], {message_json["msg_id"], {}, message_json}}});
            queue_cv.notify_one();
        }
        std::unique_lock<std::mutex> lock(queue_mutex);
        event_queue.push({EventType::EOF, std::monostate()});
        queue_cv.notify_one();
    });

    while (true) {
        Event<P> event;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [&]() { return !event_queue.empty(); });
            event = event_queue.front();
            event_queue.pop();
        }

        if (event.type == EventType::EOF) {
            break;
        }

        auto result = node->step(event, output_stream);
        if (result.has_error()) {
            throw result.error();
        }
    }

    input_thread.wait();
}