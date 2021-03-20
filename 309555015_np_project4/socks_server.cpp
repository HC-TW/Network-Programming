#include <cstdlib>
#include <iostream>
#include <fstream>
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

enum { max_length = 10000 };
boost::asio::io_context global_io_context;

void ChildHandler(int signo)
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {}
}

class connectSession : public enable_shared_from_this<connectSession>
{
public:
    connectSession(tcp::socket socket, string DSTIP, unsigned short DSTPORT, unsigned char reply[8]) : 
                    socket_(move(socket)), app_server_socket(global_io_context), resolver_(global_io_context), host(DSTIP), port(DSTPORT)
    {
        copy(reply, reply+8, reply_);
    }
    void start()
    {
        do_connect();
    }
private:
    tcp::socket socket_;
    tcp::socket app_server_socket;
    tcp::resolver resolver_;
    unsigned char browser_data[max_length];
    unsigned char app_server_data[max_length];
    string host;
    unsigned short port;
    string browser_response;
    string app_server_response;
    unsigned char reply_[8];
    void do_connect()
    {
        auto self(shared_from_this());
        app_server_socket.async_connect(tcp::endpoint(ip::address::from_string(host), port),
                                   [this, self](boost::system::error_code ec) {
                                       if (!ec)
                                       {
                                           do_reply();
                                       }
                                   });
    }
    void do_reply()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(reply_, 8),
                                 [this, self](boost::system::error_code ec, size_t /*length*/) {
                                     if (!ec)
                                     {
                                         do_read_app_server();
                                         do_read_browser();
                                     }
                                 });
    }
    void do_read_browser()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(browser_data, max_length),
                                [this, self](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        do_write_app_server(length);
                                    }
                                });
    }
    void do_read_app_server()
    {
        auto self(shared_from_this());
        app_server_socket.async_read_some(boost::asio::buffer(app_server_data, max_length),
                                [this, self](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        do_write_browser(length);
                                    }
                                });
    }
    void do_write_browser(size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(app_server_data, length),
                                [this, self](boost::system::error_code ec, size_t /*length*/) {
                                    if (!ec)
                                    {
                                        do_read_app_server();
                                    }
                                });
    }
    void do_write_app_server(size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(app_server_socket, boost::asio::buffer(browser_data, length),
                                [this, self](boost::system::error_code ec, size_t /*length*/) {
                                    if (!ec)
                                    {
                                        do_read_browser();
                                    }
                                });
    }
};

class bindSession : public enable_shared_from_this<bindSession>
{
public:
    bindSession(tcp::socket socket, unsigned char reply[8]) : 
                socket_(move(socket)), app_server_socket(global_io_context), acceptor_(global_io_context)
    {
        copy(reply, reply+8, reply_);
        unsigned short port = app_server_socket.local_endpoint().port();
        reply_[2] = (unsigned char) (port / 256);
        reply_[3] = (unsigned char) (port % 256);
    }
    void start()
    {
        do_reply();
        do_accept();
    }

private:
    tcp::socket socket_;
    tcp::socket app_server_socket;
    tcp::acceptor acceptor_;
    unsigned char browser_data[max_length];
    unsigned char app_server_data[max_length];
    string browser_response;
    string app_server_response;
    unsigned char reply_[8];

    void do_reply()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(reply_, 8),
                                [self](boost::system::error_code ec, size_t /*length*/) {
                                    if (!ec)
                                    {
                                    }
                                });
    }
    void do_accept()
    {
        acceptor_.async_accept(app_server_socket,
            [this](boost::system::error_code ec) {
                if (!ec)
                {
                    do_reply();
                    do_read_app_server();
                    do_read_browser();
                }
                do_accept();
            });
    }
    void do_read_browser()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(browser_data, max_length),
                                [this, self](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        do_write_app_server(length);
                                    }
                                });
    }
    void do_read_app_server()
    {
        auto self(shared_from_this());
        app_server_socket.async_read_some(boost::asio::buffer(app_server_data, max_length),
                                [this, self](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        do_write_browser(length);
                                    }
                                });
    }
    void do_write_browser(size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(app_server_data, length),
                                [this, self](boost::system::error_code ec, size_t /*length*/) {
                                    if (!ec)
                                    {
                                        do_read_app_server();
                                    }
                                });
    }
    void do_write_app_server(size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(app_server_socket, boost::asio::buffer(browser_data, length),
                                [this, self](boost::system::error_code ec, size_t /*length*/) {
                                    if (!ec)
                                    {
                                        do_read_browser();
                                    }
                                });
    }
};

class socks4Session : public enable_shared_from_this<socks4Session>
{
public:
    socks4Session(tcp::socket socket) : socket_(move(socket)), resolver_(global_io_context), firewall("socks.conf")
    {
    }

    void start()
    {
        do_read();
    }

private:
    tcp::socket socket_;
    tcp::resolver resolver_;
    unsigned char VN, CD, request[1024], reply[8];
    unsigned short DSTPORT;
    string DSTIP;
    ifstream firewall;

    void do_resolve(int length)
    {
        auto self(shared_from_this());
        string host = "";
        for (int i = 9; i < length-1; i++)
        {
            host += request[i];
        }
        resolver_.async_resolve(host, to_string(DSTPORT),
                                [this, self](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
                                    if (!ec)
                                    {
                                        DSTIP = endpoints->endpoint().address().to_string();
                                        do_stuff();
                                    }
                                });
    }
    void do_read()
    {
        auto self(shared_from_this());
        reply[0] = 0x00;
        reply[1] = 0x5B;
        for (int i = 2; i < 8; i++)
        {
            reply[i] = 0x00;
        }
        socket_.async_read_some(boost::asio::buffer(request, 1024),
                                [this, self](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        // fetch request info
                                        VN = request[0];
                                        CD = request[1];
                                        DSTPORT = request[2] << 8 | request[3];
                                        if (request[4] == 0x00 && request[5] == 0x00 && request[6] == 0x00 && request[7] != 0x00)
                                        {
                                            do_resolve(length);
                                        }
                                        else
                                        {  
                                            DSTIP = to_string((unsigned int)request[4]) + '.' + to_string((unsigned int)request[5]) + '.' + to_string((unsigned int)request[6]) + '.' + to_string((unsigned int)request[7]);
                                            do_stuff();
                                        }    
                                    }
                                });
    }
    void do_stuff()
    {
        if (VN != 0x04)
        {
            cerr << "Not socks4 request." << endl;
            //exit(EXIT_FAILURE);
        }
        // check if the requested address is in firewall rule
        string line;
        while (getline(firewall, line))
        {
            vector<string> split_vector;
            vector<string> addr_str;
            unsigned char address[4];

            split(split_vector, line, is_any_of(" "), token_compress_on);
            split(addr_str, split_vector[2], is_any_of("."), token_compress_on);
            for (int i = 0; i < 4; i++)
            {
                address[i] = addr_str[i] == "*" ? 0x00: (unsigned char)stoi(addr_str[i]);
            }

            if ((split_vector[1] == "c" && CD == 0x01) || (split_vector[1] == "b" && CD == 0x02)) 
            {
                if ((address[0] == request[4] || address[0] == 0x00) && (address[1] == request[5] || address[1] == 0x00) && (address[2] == request[6] || address[2] == 0x00) && (address[3] == request[7] || address[3] == 0x00))
                {
                    reply[1] = 0x5A;
                    break;
                }
            }
        }
        firewall.close();
        // print request info
        tcp::endpoint remote_ep = socket_.remote_endpoint();
        cout << "<S_IP>: " << remote_ep.address() << endl;
        cout << "<S_PORT>: " << remote_ep.port() << endl;
        cout << "<D_IP>: " << DSTIP << endl;
        cout << "<D_PORT>: " << DSTPORT << endl;
        cout << "<Command>: " << (CD == 0x01 ? "CONNECT" : "BIND") << endl;
        cout << "<Reply>: " << (reply[1] == 0x5A ? "Accept" : "Reject") << endl << endl;
        if (reply[1] == 0x5B) // reject the request
        {
            socket_.close();
            exit(EXIT_SUCCESS);
        }
        // connect or bind operation
        if (CD == 0x01)
        {
            make_shared<connectSession>(move(socket_), DSTIP, DSTPORT, reply)->start();
        }
        else if (CD == 0x02)
        {
            make_shared<bindSession>(move(socket_), reply)->start();
        }
    }
};

class socks4Server
{
public:
    socks4Server(short port)
        : acceptor_(global_io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    tcp::acceptor acceptor_;
    int connection_count = 0;
    void do_accept()
    {
        acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec)
            {
                if (connection_count < 2)
                {   
                    connection_count++;
                    cout << connection_count << endl;
                    global_io_context.notify_fork(boost::asio::io_context::fork_prepare);
                    if (fork() == 0)
                    {
                        global_io_context.notify_fork(boost::asio::io_context::fork_child);
                        make_shared<socks4Session>(move(socket))->start();
                        acceptor_.close();
                    }
                    else
                    {
                        global_io_context.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                        do_accept();
                    }
                }
                else
                {
                    cout << "reach connection limit!" << endl;
                    do_accept();
                }
            }
            else
            {
                do_accept();
            }
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
        signal(SIGCHLD, ChildHandler);
        socks4Server s(atoi(argv[1]));
        global_io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}