#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost::asio;
using namespace boost::algorithm; 
using boost::asio::ip::tcp;

string escape(string s)
{
    string escaped="";
    for (auto &c : s)
    {
        escaped += "&#" + to_string(int(c)) + ";";
    }
    return escaped;
}

void outputShell(string session, string content)
{
    string escaped = escape(content);
    cout << "<script>document.getElementById('" + session + "').innerHTML += '" + escaped + "';</script>\n" << flush;
}

void outputCommand(string session, string content)
{
    string escaped = escape(content);
    cout << "<script>document.getElementById('" + session + "').innerHTML += '<b>" + escaped + "</b>';</script>\n" << flush;
}

void consoleOutput(map<int, vector<string>> queryInfo)
{
    cout << "Content-type: text/html" << endl << endl << flush;
    cout << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "  <head>\n"
         << "    <meta charset=\"UTF-8\" />\n"
         << "    <title>NP Project 3 Sample Console</title>\n"
         << "    <link\n"
         << "      rel=\"stylesheet\"\n"
         << "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
         << "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
         << "      crossorigin=\"anonymous\"\n"
         << "    />\n"
         << "    <link\n"
         << "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
         << "      rel=\"stylesheet\"\n"
         << "    />\n"
         << "    <link\n"
         << "      rel=\"icon\"\n"
         << "      type=\"image/png\"\n"
         << "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
         << "    />\n"
         << "    <style>\n"
         << "      * {\n"
         << "        font-family: 'Source Code Pro', monospace;\n"
         << "        font-size: 1rem !important;\n"
         << "      }\n"
         << "      body {\n"
         << "        background-color: #212529;\n"
         << "      }\n"
         << "      pre {\n"
         << "        color: #cccccc;\n"
         << "      }\n"
         << "      b {\n"
         << "      color: #01b468;\n"
         << "      }\n"
         << "    </style>\n"
         << "  </head>\n"
         << "  <body>\n"
         << "    <table class=\"table table-dark table-bordered\">\n"
         << "      <thead>\n"
         << "        <tr>\n"
         << "          <th scope=\"col\">" << queryInfo[0][0] << ":" << queryInfo[0][1] << "</th>\n"
         << "          <th scope=\"col\">" << queryInfo[1][0] << ":" << queryInfo[1][1] << "</th>\n"
         << "          <th scope=\"col\">" << queryInfo[2][0] << ":" << queryInfo[2][1] << "</th>\n"
         << "          <th scope=\"col\">" << queryInfo[3][0] << ":" << queryInfo[3][1] << "</th>\n"
         << "          <th scope=\"col\">" << queryInfo[4][0] << ":" << queryInfo[4][1] << "</th>\n"
         << "        </tr>\n"
         << "      </thead>\n"
         << "      <tbody>\n"
         << "        <tr>\n"
         << "          <td><pre id=\"s0\" class=\"mb-0\"></pre></td>\n"
         << "          <td><pre id=\"s1\" class=\"mb-0\"></pre></td>\n"
         << "          <td><pre id=\"s2\" class=\"mb-0\"></pre></td>\n"
         << "          <td><pre id=\"s3\" class=\"mb-0\"></pre></td>\n"
         << "          <td><pre id=\"s4\" class=\"mb-0\"></pre></td>\n"
         << "        </tr>\n"
         << "      </tbody>\n"
         << "    </table>\n"
         << "  </body>\n"
         << "</html>\n" << flush;
}

void setQueryInfo(map<int, vector<string>> &queryInfo)
{
    string query_string = getenv("QUERY_STRING");
    vector<string> split_vector;
    split(split_vector, query_string, is_any_of("&"), token_compress_on);
    for (int i = 0; i < 15; i++)
    {
        int pos = split_vector[i].find("=");
        queryInfo[i/3].push_back(pos < split_vector[i].length()-1 ? split_vector[i].substr(pos+1) : "");
    }
}

class session : public enable_shared_from_this<session> {
    public:
        session(int id, boost::asio::io_context &io_context, map<int, vector<string>> queryInfo) : 
                socket_(io_context), resolver_(io_context), sessionName("s"+to_string(id)), timer(io_context), 
                host(queryInfo[id][0]), port(queryInfo[id][1]), file("test_case/" + queryInfo[id][2])
        {
        }
        void start()
        {
            do_resolve();
        }

    private:
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
                                            outputShell(sessionName, response);
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
                                            else
                                            {
                                                do_read();
                                            }
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
            outputCommand(sessionName, line);
            boost::asio::async_write(socket_, boost::asio::buffer(line.c_str(), line.length()),
                                    [this, self](boost::system::error_code ec, size_t /*length*/) {
                                        if (!ec)
                                        {
                                            do_read();
                                        }
                                    });
        }
};

int main()
{   
    try
    {
        map<int, vector<string>> queryInfo;
        boost::asio::io_context io_context;

        setQueryInfo(queryInfo);
        consoleOutput(queryInfo);
        for (int i = 0; i < 5; i++)
        {
            make_shared<session>(i, io_context, queryInfo)->start();
        }
        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
}