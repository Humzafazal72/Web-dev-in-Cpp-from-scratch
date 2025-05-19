#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <regex>

using namespace std;

static int PORT = 8080; 

/*                              Utilities                                     */
struct HttpResponse;
unordered_map<string, HttpResponse(*)(string)> routes;

vector<string> split_string(const string& str) {
    vector<string> words;
    istringstream ss(str);
    string word;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

class HttpRequest {
public:
    string method;
    string path;
    string body;
    map<string, string> headers;

    HttpRequest(string m, string p) {
        method = m;
        path = p;
    }
};

struct HttpResponse {
    string response_code;
    string status_text;
    map<string, string> headers;
    string body;
};

/*                              Handlers                                        */
HttpResponse redirect_to_index() {
    HttpResponse response;
    response.response_code = "303";
    response.status_text = "See Other";
    response.headers["Location"] = "/";
    response.headers["Content-Type"] = "text/plain"; // Optional
    response.body = "Redirecting...";
    return response;
}


HttpResponse index(string arg) {
    ifstream html("index.html");
    ifstream file("items.txt");
    if (!html.is_open()){
        HttpResponse response;
        response.response_code = "400";
        response.status_text = "error";
        response.headers["Content-Type"] = "text/html";
        response.body = "Erro loading index.html";
        return response;    
    }

    ostringstream buffer;
    buffer << html.rdbuf();

    string lines="";string line;
    while(getline(file,line)){
        lines += "<li>" + line + 
         " <a href=\"delete_item?item=" + line + 
         "\" style=\"margin-left:10px;padding:4px 8px;background-color:#ff4d4d;color:white;border:none;border-radius:4px;text-decoration:none;font-size:0.9em;\">Delete</a></li>\n";

    }    
    HttpResponse response;
    response.response_code = "200";
    response.status_text = "OK";
    response.headers["Content-Type"] = "text/html";
    response.body = regex_replace(buffer.str(),regex("items"),lines);
    return response;
}

HttpResponse add_item(string item){
    ofstream file("items.txt", ios::app);
    file << item << "\n";
    file.close();    
    return redirect_to_index();
}

HttpResponse delete_item(string item){
    item = regex_replace(item,regex("%20")," ");
    ifstream infile("items.txt");
    string line,items="";
    while(getline(infile,line)){
        items.append(line);
        items.append("&");
    }
    items = regex_replace(items,regex(item+"&"),"");
    items = regex_replace(items,regex("&"),"\n");

    infile.close();

    ofstream outfile("items.txt",ios::out);
    outfile << items;

    return redirect_to_index();
}

/*                              Driver Function                               */
int main() {
    // Register routes
    routes["GET /"] = &index;
    routes["GET /delete_item"]=&delete_item;
    routes["POST /add_item"] = &add_item;


    // init Socket 
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        cerr << "Failed to create socket" << endl;
        return 1;
    }

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        cerr << "setsockopt failed" << endl;
        return 1;
    }

    // Bind 
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address))) {
        cerr << "Failed to bind" << endl;
        return 1;
    }

    // Listen
    if (listen(socket_fd, 1)) {
        cerr << "Failed to listen" << endl;
        return 1;
    }
    cout << "Listening on 127.0.0.1:8080..." << endl;

    while (true) {
        int client_fd = accept(socket_fd, nullptr, nullptr);
        char buffer[1024] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer));

        // Parsing and Response
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            istringstream stream(buffer);
            string line;
            getline(stream, line);

            if (line.find("/favicon.ico") == string::npos && line.find("GET") != string::npos) {
                vector<string> header = split_string(line);
                string arg=" ";
                string route_key;
                if (header[1].find("?")==string::npos){
                    route_key = header[0] + " " + header[1];
                } else{
                    int pos = header[1].find("?"); 
                    route_key = header[0] + " " + header[1].substr(0,pos);
                    arg = header[1].substr(pos+6,header[1].length());
                    cout<< route_key << " " << arg << endl;
                }

                if (routes.find(route_key) != routes.end()) {
                    HttpResponse response = routes[route_key](arg);
                    string res = "HTTP/1.1 " + response.response_code + " " + response.status_text + "\r\n";
                    for (const auto& [key, value] : response.headers) {
                        res += key + ": " + value + "\r\n";
                    }
                    res += "\r\n" + response.body;

                    send(client_fd, res.c_str(), res.size(), 0);
                } else {
                    string not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\n404 Not Found";
                    send(client_fd, not_found.c_str(), not_found.size(), 0);
                }
            }
            
            else if (line.find("POST")!= string::npos){
                string item_;
                while(getline(stream,item_)){
                    continue;
                }
                string item = item_.substr(5,item_.size());
                vector<string> header = split_string(line); 
                string route_key = header[0] + " " + header[1];
                if(routes.find(route_key)!=routes.end()){
                    HttpResponse response = routes[route_key](regex_replace(item,regex("\\+")," "));
                    string res = "HTTP/1.1 " + response.response_code + " " + response.status_text + "\r\n";
                    for (const auto& [key, value] : response.headers) {
                        res += key + ": " + value + "\r\n";
                    }
                    res += "\r\n" + response.body;
                    send(client_fd, res.c_str(),res.size(),0);
                } else{
                    string not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\n404 Not Found";
                    send(client_fd, not_found.c_str(), not_found.size(), 0);
                }
            }

            else {
                cout << "Unsupported method." << endl;
            }
            close(client_fd);
        } else {
            cerr << "Failed to read data" << endl;
            close(client_fd);
        }
    }
    return 0;
}