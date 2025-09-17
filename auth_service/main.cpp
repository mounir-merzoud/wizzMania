#include <httplib.h>
#include <iostream>

int main() {
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    std::cout << "Server running on http://localhost:8081/health" << std::endl;

    svr.new_task_queue = [] { return new httplib::ThreadPool(1); }; // 1 thread
    svr.listen("localhost", 8081);
}
