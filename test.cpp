#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<cstdlib>

using namespace std;

int main(){
    int sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd < 0){
        perror("socket failed");
        exit(1);
    }
    cout << "Server waiting on port 9090" << endl;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9090);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(sock_fd, (sockaddr*)&addr, sizeof(addr));
    listen(sock_fd,5);
    int client_fd = accept(sock_fd,NULL,NULL);
    if(client_fd >= 0){
        cout << "Connected with client" << endl;
    }
    char buffer[1024];
    while(true){
        memset(buffer, 0 , sizeof(buffer));
        int r = read(client_fd,buffer,sizeof(buffer));
        if(r <= 0) break;

        cout << "Client : " << buffer << endl;
        send(client_fd,buffer,r,0);
    }
    close(client_fd);
}
