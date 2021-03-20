#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>  
#include <boost/algorithm/string/split.hpp>
 
using namespace std;
using namespace boost::asio;
using namespace boost::algorithm; 
using boost::asio::ip::tcp;

string HTTP_RESPONSE = "HTTP/1.1 200 OK";

string escape(string s)
{
    string escaped="";
    for (auto &c : s)
    {
        escaped += "&#" + to_string(int(c)) + ";";
    }
    return escaped;
}

string outputShell(string session, string content)
{
    string output = "";
    output += "<script>document.getElementById('" + session + "').innerHTML += '" + escape(content) + "';</script>\n";
    return output;
}

string outputCommand(string session, string content)
{
    string output = "";
    output += "<script>document.getElementById('" + session + "').innerHTML += '<b>" + escape(content) + "</b>';</script>\n";
    return output;
}

string panelOutput()
{
    int N_SERVERS = 5;
    string FORM_METHOD = "GET";
    string FORM_ACTION = "console.cgi";
    string TEST_CASE_DIR = "test_case";
    string DOMAIN_ = "cs.nctu.edu.tw";

    string test_case_menu = "";
    string host_menu = "";
    string panel = "";

    for (int i = 1; i <= 10; i++)
    {
        test_case_menu += "<option value=\"t" + to_string(i) + ".txt\">t" + to_string(i) + ".txt</option>";
    }
    for (int i = 1; i <= 12; i++)
    {
        host_menu += "<option value=\"nplinux" + to_string(i) + "." + DOMAIN_ + "\">nplinux" + to_string(i) + "</option>";
    }
    panel += "HTTP / 1.1 200 OK\n";
    panel += "Content-type: text/html\n\n";
    panel += "<!DOCTYPE html>\n";
    panel += "<html lang=\"en\">\n";
    panel += "  <head>\n";
    panel += "    <title>NP Project 3 Panel</title>\n";
    panel += "    <link\n";  
    panel += "      rel=\"stylesheet\"\n";
    panel += "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    panel += "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    panel += "      crossorigin=\"anonymous\"\n";
    panel += "    />\n";
    panel += "    <link\n";
    panel += "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    panel += "      rel=\"stylesheet\"\n";
    panel += "    />\n";
    panel += "    <link\n";
    panel += "      rel=\"icon\"\n";
    panel += "      type=\"image/png\"\n";
    panel += "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n";
    panel += "    />\n";
    panel += "    <style>\n";
    panel += "      * {\n";
    panel += "        font-family: 'Source Code Pro', monospace;\n";
    panel += "      }\n";
    panel += "    </style>\n";
    panel += "  </head>\n";
    panel += "  <body class=\"bg-secondary pt-5\">\n";

    panel += "    <form action=\"" + FORM_ACTION + "\" method=\"" + FORM_METHOD + "\">\n";
    panel += "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n";
    panel += "        <thead class=\"thead-dark\">\n";
    panel += "          <tr>\n";
    panel += "            <th scope=\"col\">#</th>\n";
    panel += "            <th scope=\"col\">Host</th>\n";
    panel += "            <th scope=\"col\">Port</th>\n";
    panel += "            <th scope=\"col\">Input File</th>\n";
    panel += "          </tr>\n";
    panel += "        </thead>\n";
    panel += "        <tbody>\n";
                      
    for (int i = 0; i < N_SERVERS; i++)
    {
        panel += "          <tr>\n";
        panel += "            <th scope=\"row\" class=\"align-middle\">Session " + to_string(i+1) + "</th>\n";
        panel += "            <td>\n";
        panel += "              <div class=\"input-group\">\n";
        panel += "                <select name=\"h" + to_string(i) + "\" class=\"custom-select\">\n";
        panel += "                  <option></option>" + host_menu + "\n";
        panel += "                </select>\n";
        panel += "                <div class=\"input-group-append\">\n";
        panel += "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n";
        panel += "                </div>\n";
        panel += "              </div>\n";
        panel += "            </td>\n";
        panel += "            <td>\n";
        panel += "              <input name=\"p" + to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\n";
        panel += "            </td>\n";
        panel += "            <td>\n";
        panel += "              <select name=\"f" + to_string(i) + "\" class=\"custom-select\">\n";
        panel += "                <option></option>\n";
        panel += "                " + test_case_menu + "\n";
        panel += "              </select>\n";
        panel += "            </td>\n";
        panel += "          </tr>\n";
    }
    panel += "          <tr>\n";
    panel += "            <td colspan=\"3\"></td>\n";
    panel += "            <td>\n";
    panel += "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n";
    panel += "            </td>\n";
    panel += "          </tr>\n";
    panel += "        </tbody>\n";
    panel += "      </table>\n";
    panel += "    </form>\n";
    panel += "  </body>\n";
    panel += "</html>\n";

    return panel;
}
string consoleOutput(map<int, vector<string>> queryInfo)
{
    string console;
    console += "HTTP / 1.1 200 OK\n";
    console += "Content-type: text/html\n\n";
    console += "<!DOCTYPE html>\n";
    console += "<html lang=\"en\">\n";
    console += "  <head>\n";
    console += "    <meta charset=\"UTF-8\" />\n";
    console += "    <title>NP Project 3 Sample Console</title>\n";
    console += "    <link\n";
    console += "      rel=\"stylesheet\"\n";
    console += "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    console += "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    console += "      crossorigin=\"anonymous\"\n";
    console += "    />\n";
    console += "    <link\n";
    console += "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    console += "      rel=\"stylesheet\"\n";
    console += "    />\n";
    console += "    <link\n";
    console += "      rel=\"icon\"\n";
    console += "      type=\"image/png\"\n";
    console += "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    console += "    />\n";
    console += "    <style>\n";
    console += "      * {\n";
    console += "        font-family: 'Source Code Pro', monospace;\n";
    console += "        font-size: 1rem !important;\n";
    console += "      }\n";
    console += "      body {\n";
    console += "        background-color: #212529;\n";
    console += "      }\n";
    console += "      pre {\n";
    console += "        color: #cccccc;\n";
    console += "      }\n";
    console += "      b {\n";
    console += "      color: #01b468;\n";
    console += "      }\n";
    console += "    </style>\n";
    console += "  </head>\n";
    console += "  <body>\n";
    console += "    <table class=\"table table-dark table-bordered\">\n";
    console += "      <thead>\n";
    console += "        <tr>\n";
    console += "          <th scope=\"col\">" + queryInfo[0][0] + ":" + queryInfo[0][1] + "</th>\n";
    console += "          <th scope=\"col\">" + queryInfo[1][0] + ":" + queryInfo[1][1] + "</th>\n";
    console += "          <th scope=\"col\">" + queryInfo[2][0] + ":" + queryInfo[2][1] + "</th>\n";
    console += "          <th scope=\"col\">" + queryInfo[3][0] + ":" + queryInfo[3][1] + "</th>\n";
    console += "          <th scope=\"col\">" + queryInfo[4][0] + ":" + queryInfo[4][1] + "</th>\n";
    console += "        </tr>\n";
    console += "      </thead>\n";
    console += "      <tbody>\n";
    console += "        <tr>\n";
    console += "          <td><pre id=\"s0\" class=\"mb-0\"></pre></td>\n";
    console += "          <td><pre id=\"s1\" class=\"mb-0\"></pre></td>\n";
    console += "          <td><pre id=\"s2\" class=\"mb-0\"></pre></td>\n";
    console += "          <td><pre id=\"s3\" class=\"mb-0\"></pre></td>\n";
    console += "          <td><pre id=\"s4\" class=\"mb-0\"></pre></td>\n";
    console += "        </tr>\n";
    console += "      </tbody>\n";
    console += "    </table>\n";
    console += "  </body>\n";
    console += "</html>\n";

    return console;
}

void setQueryInfo(string query_string, map<int, vector<string>> &queryInfo)
{
    vector<string> split_vector;
    split(split_vector, query_string, is_any_of("&"), token_compress_on);
    for (int i = 0; i < 15; i++)
    {
        int pos = split_vector[i].find("=");
        queryInfo[i/3].push_back(pos < split_vector[i].length()-1 ? split_vector[i].substr(pos+1) : "");
    }
}
class consoleSession : public enable_shared_from_this<consoleSession> {
    public:
        consoleSession(boost::shared_ptr<tcp::socket> server_socket, int id, boost::asio::io_context &io_context, map<int, vector<string>> queryInfo) : 
                server_socket_(server_socket), socket_(io_context), resolver_(io_context), sessionName("s"+to_string(id)), timer(io_context), 
                host(queryInfo[id][0]), port(queryInfo[id][1]), file("test_case/" + queryInfo[id][2])
        {
        }
        void start()
        {
            do_resolve();
        }

    private:
        boost::shared_ptr<tcp::socket> server_socket_;
        tcp::socket socket_;
        tcp::resolver resolver_;
        enum { max_length = 1024 };
        char data_[max_length];
        string sessionName;
        string response;
        steady_timer timer;
        string host;
        string port;
        ifstream file;

        void do_resolve()
        {
            auto self(shared_from_this());
            resolver_.async_resolve(host, port,
                [this,self](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
                    if (!ec) 
                    {
                        do_connect(endpoints);
                    }
            });
        }
        void do_connect(tcp::resolver::results_type endpoints)
        {
            auto self(shared_from_this());
            boost::asio::async_connect(socket_, endpoints,
                [this, self](boost::system::error_code ec, tcp::endpoint) {
                    if (!ec)
                    {
                        do_read();
                    }
            });
        }
        void do_read()
        {
            auto self(shared_from_this());
            socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                    [this, self](boost::system::error_code ec, size_t length) {
                                        if (!ec)
                                        {
                                            response="";
                                            for (int i = 0; i < length; i++)
                                            {
                                                response += data_[i];
                                            }
                                            string output = outputShell(sessionName, response);
                                            boost::asio::async_write(*server_socket_, boost::asio::buffer(output.c_str(), output.length()),
                                                                    [this, self](boost::system::error_code ec, size_t /*length*/) {
                                            });
                                            if (response.find("%") != string::npos)
                                            {
                                                timer.expires_after(boost::asio::chrono::seconds(1));
                                                timer.async_wait(
                                                    [this, self](boost::system::error_code ec) {
                                                        if (!ec)
                                                        {
                                                            do_write();
                                                        }
                                                });
                                            }
                                            do_read();
                                        }
                                    });
        }
        void do_write()
        {
            auto self(shared_from_this());
            string line;
            getline(file, line);
            if (line == "exit")
                file.close();
            line += "\n";
            
            boost::asio::async_write(socket_, boost::asio::buffer(line.c_str(), line.length()),
                                    [this, self, line](boost::system::error_code ec, size_t /*length*/) {
                                        if (!ec)
                                        {
                                            string output = outputCommand(sessionName, line);
                                            boost::asio::async_write(*server_socket_, boost::asio::buffer(output.c_str(), output.length()),
                                            [this, self](boost::system::error_code ec, size_t /*length*/) {
                                                if (!ec)
                                                {
                                                    do_read();
                                                }
                                            });
                                        }
                                    });
        }
};
class serverSession : public enable_shared_from_this<serverSession> {
    public:
        serverSession(tcp::socket socket) : socket_(move(socket))
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
                                            do_write();
                                        }
                                    });
        }

        void do_write()
        {
            auto self(shared_from_this());
            if (request.find("GET /panel.cgi") != string::npos)
            {
                string panel = panelOutput();
                boost::asio::async_write(socket_, boost::asio::buffer(panel, panel.length()),
                [this, self](boost::system::error_code ec, size_t /*length*/) {
                    if (!ec)
                    {
                        do_read();
                    }
                });
            }
            else if (request.find("GET /console.cgi") != string::npos)
            {
                vector<string> split_vector;
                vector<string> request_method;
                vector<string> query_string;
                map<int, vector<string>> queryInfo;
                
                split(split_vector, request, is_any_of("\r\n"), token_compress_on);
                split(request_method, split_vector[0], is_any_of(" "), token_compress_on);
                split(query_string, request_method[1], is_any_of("?"), token_compress_on);
                setQueryInfo(query_string[1], queryInfo);
                string console = consoleOutput(queryInfo);
                
                boost::asio::async_write(socket_, boost::asio::buffer(console, console.length()),
                [this, self, queryInfo](boost::system::error_code ec, size_t /*length*/) {
                    if (!ec)
                    {
                        try
                        {
                            boost::shared_ptr<tcp::socket> server_socket(&socket_);
                            boost::asio::io_context io_context;
                            for (int i = 0; i < 5; i++)
                            {
                                make_shared<consoleSession>(server_socket, i, io_context, queryInfo)->start();
                            }
                            io_context.run();
                        }
                        catch (exception &e)
                        {
                            cerr << "Exception: " << e.what() << "\n";
                        }
                        do_read();
                    }
                });
            }                
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
                        make_shared<serverSession>(move(socket))->start();
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