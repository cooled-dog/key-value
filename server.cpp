#include<iostream>
#include<cstdlib>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string>
#include<cstring>
#include<unordered_map>
#include<thread>
#include<mutex>
#include<semaphore.h>
#include<chrono>
#include<list>

using namespace std;

class LRUcache{
    private:
        mutex mtx;
        unordered_map<string,chrono::steady_clock::time_point> expiry;
        unordered_map<string, pair<string,list<string>::iterator>> store;
        
        int capacity;
        list<string> dll;

        void checkRemove(){
            if(store.size() > capacity){
                string temp = dll.back();
                dll.pop_back();
                store.erase(temp);
                expiry.erase(temp);
            }
        }
    public:
        LRUcache(int cap) : capacity(cap){}
        void funcSET(char buffer[], int fd);
        void funcGET(char buffer[], int fd);
        void funcDEL(char buffer[], int fd);
        void funcEXPIRE(char buffer[], int fd);
};

class KVstore{
    private:
        LRUcache cache;
        
    public:
        KVstore(int cap) : cache(cap){}
        void handleClient(int client_fd);
};

void KVstore::handleClient(int client_fd){
    char buffer[1024];
    while(true){
        memset(buffer, 0 , sizeof(buffer));
        
        int r = read(client_fd,buffer, sizeof(buffer));
        if(r <= 0) break;

        char cmd[10] = {0} , key[100] = {0} , value[100] = {0};
        sscanf(buffer,"%s", cmd);
        
        if(strcmp(cmd,"SET") == 0){
            cache.funcSET(buffer,client_fd);
        }
        else if(strcmp(cmd,"GET") == 0){ 
            cache.funcGET(buffer,client_fd);
        }
        else if(strcmp(cmd,"DEL") == 0){
            cache.funcDEL(buffer,client_fd);
        }
        else if(strcmp(cmd,"EXPIRE") == 0){
            cache.funcEXPIRE(buffer,client_fd);
        }
        else send(client_fd, "command not found!!\n", 20, 0);
    }
    close(client_fd);
}

void LRUcache::funcSET(char buffer[], int fd){
    char key[100] , value[100];
    sscanf(buffer, "%*s %s %[^\n]", key, value);
    {
        lock_guard<mutex> lock(mtx);
        if(store.find(key) != store.end())
            dll.erase(store[key].second);

        dll.push_front(key);
        store[key] = {value, dll.begin()};
        checkRemove();
    }
    send(fd, "OK\n", 3, 0);
}

void LRUcache::funcGET(char buffer[], int fd){
    char key[100];
    sscanf(buffer,"%*s %s", key);
    
    lock_guard<mutex> lock(mtx);

    auto it = store.find(key);
    if(it == store.end()){
        send(fd, "NULL\n", 5, 0);
        return;
    }
    auto exp = expiry.find(key);
    if(exp != expiry.end() && chrono::steady_clock::now() >= exp->second){
        dll.erase(it->second.second);
        store.erase(key);
        expiry.erase(key);
        send(fd, "NULL\n", 5, 0);
        return;
    }
    else{
        send(fd, it->second.first.c_str(), it->second.first.size(), 0);
        send(fd, "\n", 1, 0);
    }
    dll.erase(it->second.second);
    dll.push_front(key);
    it->second.second = dll.begin();
}

void LRUcache::funcDEL(char buffer[], int fd){
    char key[100];
    sscanf(buffer,"%*s %s", key);
    {
        lock_guard<mutex> lock(mtx);
        if(store.find(key) != store.end()){
            auto it = store.find(key);
            dll.erase(it->second.second);
            store.erase(it);
        }
        if(expiry.find(key) != expiry.end())
            expiry.erase(key);
    }
    send(fd, "OK\n", 3, 0);
}

void LRUcache::funcEXPIRE(char buffer[], int fd){
    char key[100];
    int value;
    sscanf(buffer, "%*s %s %d", key, &value);
    {
        lock_guard<mutex> lock(mtx);
        auto it = store.find(key);
        if(it == store.end()){
            send(fd, "entry not found!\n", 17, 0);
            return;
        }
        expiry[key] = chrono::steady_clock::now() + chrono::seconds(value);
    }
    send(fd, "OK\n", 3, 0);
}

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
    
    KVstore kv(2);
    while(true){
        int client_fd = accept(sock_fd,NULL,NULL);
        if(client_fd < 0) perror("accept failed");
        else printf("Connected\n ");

        thread t(&KVstore::handleClient,&kv,client_fd);
        t.detach();
    }
}