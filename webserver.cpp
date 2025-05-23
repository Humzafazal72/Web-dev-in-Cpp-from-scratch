#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <regex>
#include <random>
#include <cstring>

using namespace std;

static int PORT = 8080; 

/*                              Utilities                                     */
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

struct User{
    char username[50];
    char password[50];
};

unordered_map<string, HttpResponse(*)(string)> routes;

vector<string> split_string_(const string& str) {
    vector<string> words;
    istringstream ss(str);
    string word;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

vector<string> split_string(const string& str, const string& delimiter) {
    vector<string> output;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != string::npos) {
        output.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    output.push_back(str.substr(start)); // Add the remaining part

    return output;
}

bool check_cookie(char* buffer){
    string line;
    istringstream stream(buffer);
    while(getline(stream,line)){
        if(line.find("Cookie:")!=string::npos){
            return true;
        } 
    }
    return false;
}

bool find_user(string username, string password){
    ifstream inFile("Users.dat");
    User current;
    while (inFile.read(reinterpret_cast<char*>(&current),sizeof(User))){
        if(password == current.password && username == current.username){
            return true;
        }
    }
    return false;
}

string generate_session_id() {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    string session_id;
    for (size_t i = 0; i < 10; ++i) {
        session_id += charset[dist(generator)];
    }
    return session_id;
}

/*                              Handlers                                        */
HttpResponse redirect_to(string location) {
    HttpResponse response;
    response.response_code = "303";
    response.status_text = "See Other";
    response.headers["Location"] = "/"+location;
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

HttpResponse login(string arg) {
    ifstream html("login.html");
    if (!html.is_open()){
        HttpResponse response;
        response.response_code = "400";
        response.status_text = "error";
        response.headers["Content-Type"] = "text/html";
        response.body = "Erro loading login.html";
        return response;    
    }

    ostringstream buffer;
    buffer << html.rdbuf();
        
    HttpResponse response;
    response.response_code = "200";
    response.status_text = "OK";
    response.headers["Content-Type"] = "text/html";
    response.body = buffer.str();
    return response;
}

HttpResponse login_handler(string formData) {
    vector<string> args = split_string(formData,"&");
    string username = split_string(args[0],"=")[1];
    string password = split_string(args[1],"=")[1];
    

    bool user_exists = find_user(username,password);
    if(user_exists){
        string session_id = generate_session_id(); 
        HttpResponse response;
        response.response_code = "200";
        response.status_text = "ok";
        response.headers["Content-Type"] = "text/html";
        response.headers["Location"] = "/";
        response.headers["Set-Cookie"] = "session_id=" + session_id + "; Path=/; HttpOnly";
        return response;
    }
    HttpResponse res;
    return res;
}

HttpResponse signup(string arg) {
    ifstream html("signup.html");
    if (!html.is_open()){
        HttpResponse response;
        response.response_code = "400";
        response.status_text = "error";
        response.headers["Content-Type"] = "text/html";
        response.body = "Erro loading signup.html";
        return response;    
    }

    ostringstream buffer;
    buffer << html.rdbuf();
        
    HttpResponse response;
    response.response_code = "200";
    response.status_text = "OK";
    response.headers["Content-Type"] = "text/html";
    response.body = buffer.str();
    return response;
}

HttpResponse signup_handler(string formData){
    vector<string> args = split_string(formData,"&");
        
    string username = split_string(args[0],"=")[1];
    string password = split_string(args[1],"=")[1];
    string cpassword = split_string(args[2],"=")[1];

    if(password==cpassword){
        User u = {username,password};
        ofstream outFile("Users.dat",ios::binary);
        outFile.write(reinterpret_cast<char*>(&u), sizeof(User));
        outFile.close();
        return redirect_to("login");
    }else{
        HttpResponse res;
        res.response_code = "400";
        res.status_text = "error";
        res.headers["Content-Type"] = "text/html";
        res.body = "The passwords didn't match";
        return res;    
    }
    HttpResponse res;
    return res;
}

HttpResponse add_item(string formData){
    string item_ = formData.substr(5,formData.size());
    string item = regex_replace(item_,regex("\\+")," "); 
    ofstream file("items.txt", ios::app);
    file << item << "\n";
    file.close();    
    return redirect_to("");
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

    return redirect_to("");
}

/*                              Driver Function                               */
int main() {
    // Register routes
    routes["GET /"] = &index;
    routes["GET /delete_item"] = &delete_item;
    routes["POST /add_item"] = &add_item;
    routes["POST /login"] = &login;
    routes["POST /login_handler"] = &login_handler;
    routes["GET /signup"] = &signup;
    routes["POST /signup_handler"] = &signup_handler;

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
            istringstream stream(buffer),stream2(buffer);
            string line;
            getline(stream, line);
            
            if (line.find("/favicon.ico") == string::npos && line.find("GET") != string::npos) {
                bool cookie = check_cookie(buffer);
                if(!cookie && line.find("signup")==string::npos){
                    HttpResponse response = login(" ");
                    string res = "HTTP/1.1 " + response.response_code + " " + response.status_text + "\r\n";
                    for (const auto& [key, value] : response.headers) {
                        res += key + ": " + value + "\r\n";
                    }
                    res += "\r\n" + response.body;
                    send(client_fd, res.c_str(),res.size(),0);
                }else{
                vector<string> header = split_string_(line);
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
            }
            else if (line.find("POST")!= string::npos){
                string formData;
                while(getline(stream,formData)){
                    continue;
                }
                vector<string> header = split_string_(line); 
                string route_key = header[0] + " " + header[1];
                
                if(routes.find(route_key)!=routes.end()){
                    HttpResponse response = routes[route_key](formData);
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