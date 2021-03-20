#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>  
#include <boost/algorithm/string/split.hpp>
 
 
using namespace std;
using namespace boost::asio;
using namespace boost::algorithm; 
using boost::asio::ip::tcp;

string HTTP_RESPONSE = "HTTP/1.1 200 OK";

void ChildHandler(int signo)
{
    int status; 
    while(waitpid(-1, &status, WNOHANG) > 0) {}
}
class session : public enable_shared_from_this<session> {
    public:
        session(tcp::socket socket) : socket_(move(socket))
        {
        }

        void start()
        {
            do_read();
        }

    private:
        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
        string request;
        void do_read()
        {
            auto self(shared_from_this());
            socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                    [this, self](boost::system::error_code ec, size_t length) {
                                        if (!ec)
                                        {
                                            request="";
                                            for (int i = 0; i < length; i++)
                                            {
                                                request += data_[i];
                                            }
                                            do_write(length);
                                        }
                                    });
        }

        void do_write(size_t length)
        {
            auto self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(HTTP_RESPONSE, HTTP_RESPONSE.length()),
                                    [this, self](boost::system::error_code ec, size_t /*length*/) {
                                        if (!ec)
                                        {
                                            if (fork() == 0)
                                            {
                                                tcp::endpoint local_ep = socket_.local_endpoint();
                                                tcp::endpoint remote_ep = socket_.remote_endpoint();
                                                setenv("SERVER_ADDR", local_ep.address().to_string().c_str(), 1);
                                                setenv("SERVER_PORT", to_string(local_ep.port()).c_str(), 1);
                                                setenv("REMOTE_ADDR", remote_ep.address().to_string().c_str(), 1);
                                                setenv("REMOTE_PORT", to_string(remote_ep.port()).c_str(), 1);

                                                vector<string> split_vector;
                                                vector<string> request_method;
                                                vector<string> query_string;
                                                vector<string> http_host;
                                                split(split_vector, request, is_any_of("\r\n"), token_compress_on);
                                                split(request_method, split_vector[0], is_any_of(" "), token_compress_on);
                                                split(query_string, request_method[1], is_any_of("?"), token_compress_on);
                                                split(http_host, split_vector[1], is_any_of(" "), token_compress_on);
                                                setenv("REQUEST_METHOD", request_method[0].c_str(), 1);
                                                setenv("REQUEST_URI", request_method[1].c_str(), 1);
                                                setenv("QUERY_STRING", query_string[1].c_str(), 1);
                                                setenv("SERVER_PROTOCOL", request_method[2].c_str(), 1);
                                                setenv("HTTP_HOST", http_host[1].c_str(), 1);

                                                string cgi = "." + query_string[0];
                                                char *argv[2];
                                                strcpy(argv[0], cgi.c_str());
                                                argv[1] = NULL;

                                                dup2(socket_.native_handle(), STDIN_FILENO);
                                                dup2(socket_.native_handle(), STDOUT_FILENO);
                                                dup2(socket_.native_handle(), STDERR_FILENO);
                                                if (execvp(argv[0], argv) == -1)
                                                {
                                                    fprintf(stdout, "Unknown command: [%s].\n", argv[0]);
                                                    exit(EXIT_FAILURE);
                                                }
                                            }
                                            socket_.close();
                                        }
                                    });
        }
};

class server {
    tcp::acceptor acceptor_;
    public:
        server(boost::asio::io_context &io_context, short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
            do_accept();
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket) {
                    if (!ec)
                    {
                        make_shared<session>(move(socket))->start();
                    }
                    do_accept();
                });
        }
};

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        signal(SIGCHLD,ChildHandler);
        boost::asio::io_context io_context;

        server s(io_context, atoi(argv[1]));

        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}