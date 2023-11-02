#include "http_server.hpp"
#include "searcher.hpp"
#include "log.hpp"

int main()
{
    ns_httpserver::Server svr(8081);
    svr.RunModule();

    return 0;
}