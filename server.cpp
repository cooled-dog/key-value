#include<iostream>
#include<cstdlib>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string>
#include<cstring>
#include<unordered_map>
using namespace std;

int main(int argc , char* argv[]){
    if(argc < 2){
        fprintf(stderr, "Port not mentioned, code terminated \n");
        exit(1);
    }
    
    int port = stoi(argv[1]);
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        perror("socket failed");
        exit(1);
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    int b = bind(sock_fd,(sockaddr*)&addr, sizeof(addr));
    if(b < 0){
        perror("bind failed");
        exit(1);
    }
    int l = listen(sock_fd, 5);
    if(l < 0){
        perror("listen failed");
        exit(1);
    }
    
    cout << "Server running on " << port << endl;
    
    unordered_map<string,string> store;
    while(true){
        int client_fd = accept(sock_fd,NULL,NULL);
        char buffer[1024];
        while(true){
            memset(buffer, 0 , sizeof(buffer));
            int r = read(client_fd,buffer, sizeof(buffer));
            if(r <= 0) break;
            char cmd[10] = {0} , key[100] = {0} , value[100] = {0};
            sscanf(buffer,"%s", cmd);

            if(strcmp(cmd, "SET") == 0){
                sscanf(buffer, "%*s %s %[^\n]", key , value);
                store[key] = value;
                send(client_fd, "OK\n", 3, 0);
            }
            else if(strcmp(cmd, "GET") == 0){
                sscanf(buffer, "%*s %s", key);
                if(store.count(key)){
                    send(client_fd, store[key].c_str(), store[key].size() , 0);
                    send(client_fd, "\n", 1, 0);
                }
                else{
                    send(client_fd, "NULL\n", 5, 0);
                }
            }
            else if(strcmp(cmd, "DEL") == 0){
                sscanf(buffer, "%*s %s", key);
                store.erase(key);
                send(client_fd,"OK\n", 3, 0);
            }
            else{
                send(client_fd,"Unknow argument \n", 17, 0);
            }
        }
        close(client_fd);
    }
}