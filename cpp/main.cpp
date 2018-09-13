#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <errno.h>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "3rd_party/json.hpp"

using namespace std;
using json = nlohmann::json;

#define HTTP_PORT 5001
#define UDP_PORT 8010

#define VALID_MIN 150
#define VALID_MAX 600

#define HTML_PATH "templates/"

int convert_to_raw(float value);

// Getting content from http packet
string get_http_content(string buf);

// Making HTTP response
string http_response(string code, string content);

// Main
int main(){
  int http_s, udp_s; // HTTP Socket, UDP Socket
  struct sockaddr_in http_addr, udp_addr; //Http sockaddr, UDP sockaddr

/*
  // udp socket standby
  if((udp_s = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("socket(udp)");
    exit(1);
  }
  memset(&udp_addr, 0, sizeof udp_addr);
  udp_addr.sin_family = AF_INET;
  udp_addr.sin_port = htons(UDP_PORT);
  udp_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
*/

  // http socket standby
  if((http_s = socket(PF_INET, SOCK_STREAM, 0)) < 0){
    cerr << strerror(errno) << endl;
    exit(1);
  }
  memset(&http_addr, 0, sizeof http_addr);
  http_addr.sin_family = AF_INET;
  http_addr.sin_port = htons(HTTP_PORT);
  http_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int yes=1;
  setsockopt(http_s, SOL_SOCKET, SO_REUSEADDR, (const char *) &yes, sizeof(yes));

  // http bind and listen
  if (bind(http_s, (struct sockaddr *) &http_addr, sizeof(http_addr)) < 0) {
    cerr << strerror(errno) << endl;
    cerr << "Note: You may have to give me permission, because this app uses PORT 80(http)." << endl;
    exit(1);
  }
  if (listen(http_s, 5) < 0) {
    cerr << strerror(errno) << endl;
    exit(1);
  }

  // Ready to receiving
  cout << "HTTP receiver started..." << endl;

  while(1){
    // Starting session
    struct sockaddr_in client_addr; //Client sockaddr
    unsigned int len = sizeof(client_addr);
    char message[1024];

    // Waiting for connection
    int http_c = accept(http_s, (struct sockaddr *) &client_addr, &len);

    // Receiving data
    memset(message, 0, sizeof(message));
    recv(http_c, message, sizeof(message), 0);

    // Parsing http request
    stringstream msgstream{message};
    string type, route, content;
    json content_json, serve_json;
    getline(msgstream, type, ' ');
    getline(msgstream, route, ' ');
    content = get_http_content(message);
    cout << "REQUEST TYPE: " << type << " ROUTE: " << route << endl;
    try {

      // 200 OK response(DEFAULT)
      string response = http_response(
        "200 OK",
        "Your signal accepted!"
      );

      // Routing and recognizing
      if(route == "/dc_motor"){
        // Json parsing
        content_json = json::parse(content.c_str());

        serve_json["type"] = "dc";
        serve_json["wheel"]["left"] = (int)(content_json["wheel"]["left"]);
        serve_json["wheel"]["right"] = (int)(content_json["wheel"]["right"]);

      }else if(route == "/servo_motor"){
        // Json parsing
        content_json = json::parse(content.c_str());

        serve_json["type"] = "servo";
        serve_json["arm"]["right"] = convert_to_raw((float)content_json["arm"]["right"]);
        serve_json["arm"]["left"] = convert_to_raw((float)content_json["arm"]["left"]);
        serve_json["head"]["pitch"] = convert_to_raw((float)content_json["head"]["pitch"]);
        serve_json["head"]["roll"] = convert_to_raw((float)content_json["head"]["roll"]);
        serve_json["head"]["yaw"] = convert_to_raw((float)content_json["head"]["yaw"]);

      }else if(route == "/voice"){
        // Json parsing
        content_json = json::parse(content.c_str());

        serve_json["type"] = "voice";
        serve_json["id"] = content_json["id"];

      }else{

        // Searches a file
        string filepath;
        if(route == "/"){
          //root is index.html
          filepath = "index.html";
        }else{
          filepath = route.substr(1);
        }
        // Puts directory data
        filepath = HTML_PATH + filepath;

        if(filepath.find("..") != string::npos){
          // Path injection detected
          // 400 Bad Request response
          string errresponse = http_response(
            "400 Bad Request",
            "Your signal denied."
          );
          send(http_c, errresponse.c_str(), (int)errresponse.size(), 0);
          cout << "ERROR: PATH INJECTION DETECTED" << endl;
          //Finishing session
          close(http_c);
          continue;
        }

        cout << filepath << endl;
        
        ifstream pathfile(filepath);
        if (pathfile.fail()) {
          // try to read html file
          filepath = filepath + ".html";
          pathfile.open(filepath, ios::in);

          if(pathfile.fail()){
            // Error reading
            string errresponse = http_response(
              "404 Not Found",
              "The file you're looking for doesn't exist."
            );
            send(http_c, errresponse.c_str(), (int)errresponse.size(), 0);
            cout << "ERROR: FILE NOT FOUND"  << endl;
            close(http_c);
            continue;
          }
        }

        // File found
        string pathdata = "";
        istreambuf_iterator<char> it(pathfile);
        istreambuf_iterator<char> last;
        string rawdata(it, last);
        pathdata = rawdata;

        // Overwriting response data
        response = http_response("200 OK", pathdata);

      }

      // Responding
      send(http_c, response.c_str(), (int)response.size(), 0);

    } catch(std::exception& e) {
      // Responding(400 Bad Request)
      cerr << "JSON PERSING ERROR" << std::endl;
      string errresponse = http_response(
        "400 Bad Request",
        "Your signal denied."
      );
      send(http_c, errresponse.c_str(), (int)errresponse.size(), 0);

      //Finishing session
      close(http_c);
      continue;
    }

    // UDP sending section
    if(sendto(udp_s, serve_json.dump().c_str(), serve_json.dump().size(), 0,
      (struct sockaddr *) &udp_addr, sizeof(udp_addr)) < 0) {
      perror("sendto(udp)");
      exit(1);
    }

    // Finishing session
    close(http_c);
  }

  close(http_s);
}

int convert_to_raw(float value){
  if(value < 0) return 0;
  return (int)((VALID_MAX - VALID_MIN) * value) + VALID_MIN;
}

string get_http_content(string buf){
  // double newline codes indicate the start of the content
  string endsignal = "\r\n\r\n";
  auto r = std::search(buf.begin(), buf.end(), endsignal.begin(), endsignal.end());
  return string(r + endsignal.size(), buf.end());
}

string http_response(string code, string content){
  return "HTTP/1.0 " + code + "\r\nContent-Length: " +
    to_string(content.size()) +
    "\r\nContent-Type: text/html\r\n\r\n" +
    content + "\r\n";
}