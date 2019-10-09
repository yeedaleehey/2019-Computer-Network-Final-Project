#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>
#define Slow_Start 0
#define Congestion_Avoidance 1
#define Fast_Recovery 2
#define BufferSize 10240
int RTT = 150;    //round trip time
int threshold = 8192;
int cwnd = 1;
int rwnd = 32768;
const int MSS = 1024;    //maximum segment size
int now_seq = 0;
char buffer[10240];
int sockfd;
short server_port;
short sending_port = 12330;
char server_ip[20];
struct sockaddr_in server_info, client_info;
socklen_t addrlen = sizeof(client_info);

typedef struct{
    short source_port;
    short dest_port;
    int seq_num;
    int ack_num;
    short head_len:4, not_use:6, u:1, ack:1, p:1, r:1, syn:1, fin:1;
    short recv_window;
    short checksum;
    short urg_data_ptr;
    int option;
    char data[MSS];
}Packet;

bool generate_loss(){
    if(rand() % 10000 == 0) return true;
    return false;
}

void initial(){
    printf("=====Parameter=====\n");
    printf("The RTT delay = %d ms\n", RTT);
    printf("The threshold = %d bytes\n", threshold);
    printf("The MSS = %d bytes\n", MSS);
    printf("The buffer size = %d bytes\n", BufferSize);
    printf("Server's IP is %s\n", server_ip);
    printf("Server is listening on port %hu\n", server_port);
}

char* getHostIP(){
    char hname[128];
    struct hostent *hent;
    gethostname(hname, sizeof(hname));
    hent = gethostbyname(hname);
    return inet_ntoa(*(struct in_addr*)(hent->h_addr_list[0]));
}

int receive_packet(Packet *packet){
    int packet_len = recvfrom(sockfd, buffer, BufferSize, 0, (struct sockaddr *)&client_info, &addrlen);
    memcpy(packet, buffer, packet_len);
    return packet_len;
}

int send_packet(int sockfd, Packet packet, int data_size){
    char pk[2048];
    int packet_size = 24 + data_size;
    memset(pk, 0, 128 * sizeof(char));
    memcpy(pk, &packet, packet_size);
    sendto(sockfd, pk, packet_size, 0, (struct sockaddr *)&client_info, sizeof(client_info));
    //printf("%d %d %d", packet.syn, packet.seq_num, packet.ack_num);
    return packet_size;
}

int transimiting_file(sockaddr_in client){
    int state = Slow_Start;
    printf("*****Slow start*****\n");
    // struct timeval timeout;
    // timeout.tv_sec = 0;
    // timeout.tv_usec = 500000; // 500 ms
    // setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    //set socket timeout
    while(true){
        bool isFinished = false;
        char filename[20];
        int file_seq = 1;//rand()%10000 + 1
        int send_offset = 1;
        int dupACKcount = 0;
        int prev_ack_num = -1;
        Packet data_packet, recv_packet;
        memset(&data_packet, 0, sizeof(data_packet));
        memset(&recv_packet, 0, sizeof(recv_packet));
        //receive file name
        receive_packet(&recv_packet);
        if(recv_packet.fin) break;
        strcpy(filename, recv_packet.data);
        FILE *fp = fopen(filename, "r");
        //get file size
        struct stat st;
        stat(filename, &st);
        printf("Start to sent %s to %s : %d, the file size of %ld bytes.\n", filename, inet_ntoa(client.sin_addr), ntohs(client.sin_port), st.st_size);
        while(true){
            int send_packet_num = 0;
            int data_len[100];
            int unsend_length = cwnd;
            int tmp_seq = file_seq;
            int tmp_ack_num = recv_packet.seq_num;
            printf("cwnd = %d, rwnd = %d, threshold = %d\n", cwnd, rwnd, threshold);
            //send packets
            while(unsend_length > 0){
                int this_packet_size = 0;
                if(unsend_length > MSS){
                    //cut packet
                    this_packet_size = MSS;
                    unsend_length -= this_packet_size;
                }else{
                    this_packet_size = unsend_length;
                    unsend_length -= this_packet_size;
                }
                data_len[send_packet_num] = fread(data_packet.data, sizeof(char), this_packet_size, fp);
                if(data_len[send_packet_num] == 0){
                    isFinished = true;
                    break;
                }
                data_packet.seq_num = tmp_seq;
                data_packet.ack_num = tmp_ack_num + 1;
                //add 0.01% packet loss
                //generate loss at 4096 bytes
                if(!generate_loss()){
                    printf("\tSend a packet at : %d byte\n", tmp_seq);
                    if(cwnd != threshold && tmp_seq == 4096){
                        printf("***Data loss at byte : %d\n", tmp_seq);
                    }else{
                        send_packet(sockfd, data_packet, data_len[send_packet_num]);
                    }
                }else{
                    printf("\tSend a packet at : %d byte **loss\n", tmp_seq);
                }
                tmp_seq += data_len[send_packet_num];
                tmp_ack_num += 1;
                send_packet_num++;
            }
            //receive send_packet_num packets
            bool no_loss = true;
            int i = 0;
            while(i < send_packet_num){
                int len = receive_packet(&recv_packet);
                rwnd = recv_packet.recv_window;
                printf("\tReceive a packet (seq_num = %d, ack_num = %d)\n", recv_packet.seq_num, recv_packet.ack_num);
                //printf("recv_packet.ack_num : %d,send_offset: %d , data_len : %d\n", recv_packet.ack_num,send_offset,data_len);
                if(len == -1){
                    //timeout
                    printf("***** Time out *****\n");
                    if(state == Slow_Start){
                        threshold = cwnd / 2;
                        cwnd = MSS;
                    }else if(state == Congestion_Avoidance){
                        printf("*****Slow start*****\n");
                        state = Slow_Start;
                        threshold = cwnd / 2;
                        cwnd = MSS;
                    }
                    break;
                }
                if(recv_packet.ack && recv_packet.ack_num == send_offset + data_len[i]){
                    file_seq += data_len[i];
                    send_offset += data_len[i];
                    i++;
                }else if(recv_packet.ack && recv_packet.ack_num < send_offset + data_len[i]){
                    dupACKcount++;
                    no_loss = false;
                    if(dupACKcount == 3){
                        printf("Received three duplicated ACKs.\n");
                        printf("*****Fast recovery*****\n");
                        cwnd = threshold = cwnd / 2;
                        fseek(fp, send_offset - 1, SEEK_SET);
                        printf("*****Congestion avoidance*****\n");
                        state = Congestion_Avoidance;
                        break;
                    }
                }else if(recv_packet.fin){
                    break;
                }
            }
            if(no_loss && isFinished){
                data_packet.fin = 1;
                send_packet(sockfd, data_packet, 0);
                break;
            }
            if(no_loss){
                if(cwnd < threshold){
                    //slow start
                    if(state != Slow_Start){
                        printf("*****Slow start*****\n");
                        state = Slow_Start;
                    }
                    cwnd = cwnd * 2;
                }else{
                    if(state != Congestion_Avoidance){
                        printf("*****Congestion avoidance*****\n");
                        state = Congestion_Avoidance;
                    }
                    cwnd += MSS;
                }
            }
            dupACKcount = 0;
        }
    }
    return 1;
}


int mylisten(){
    initial();
    //create a socket
    int clientfd = 0;
    int packet_len = 0;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1) printf("Fail to create a socket.\n");
    //set server info
    bzero(&server_info, sizeof(server_info)); //set struct content to zero
    server_info.sin_family = PF_INET; //IPv4
    server_info.sin_addr.s_addr = inet_addr("0.0.0.0");
    server_info.sin_port = htons(server_port);
    //bind to sockfd
    bind(sockfd, (struct sockaddr *)&server_info,sizeof(server_info));
    while(true){
        printf("===================\n");
        printf("Listening for client.......\n");
        printf("=====Start the  three-way handshake=====\n");
        Packet rcv_packet;
        memset(&rcv_packet, 0, sizeof(rcv_packet));
        //receive syn
        packet_len = receive_packet(&rcv_packet);
        if(rcv_packet.syn){
            printf("Receive a packet(SYN) from %s : %hu\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));
        }
        printf("\tReceive a packet (seq_num = %d, ack_num = %d)\n", rcv_packet.seq_num, rcv_packet.ack_num);
        sending_port += 1;
        //forking
        pid_t pid = fork();
        if(pid == 0){
            server_port = sending_port;
            //send syn/ack
            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockfd == -1) printf("Fail to create a socket.\n");
            //set server info
            bzero(&server_info, sizeof(server_info)); //set struct content to zero
            server_info.sin_family = PF_INET; //IPv4
            server_info.sin_addr.s_addr = inet_addr("0.0.0.0");
            server_info.sin_port = htons(server_port);
            //bind to sockfd
            bind(sockfd, (struct sockaddr *)&server_info,sizeof(server_info));


            Packet syn_ack;
            memset(&syn_ack, 0, sizeof(syn_ack));
            syn_ack.source_port = server_port;
            syn_ack.dest_port = rcv_packet.source_port;
            syn_ack.syn = 1;
            syn_ack.ack = 1;
            syn_ack.ack_num = rcv_packet.seq_num + 1;
            syn_ack.seq_num = now_seq = (short)(rand()%10000)+1;
            send_packet(sockfd, syn_ack, 0);
            printf("Send a packet(SYN/ACK) to %s : %d\n",inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));
            //receive ack
            packet_len = receive_packet(&rcv_packet);
            if(rcv_packet.ack && rcv_packet.ack_num == now_seq + 1){
                printf("Receive a packet(SYN) from %s : %hu\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));
            }
            printf("\tReceive a packet (seq_num = %d, ack_num = %d)\n", rcv_packet.seq_num, rcv_packet.ack_num);
            printf("=====Complete the three-way handshake=====\n");
            //-------transimiting files -------
            transimiting_file(client_info);
            return 0;
        }
    }
    return sockfd;
}

int main(int argc, char *argv[]){
    srand(time(NULL));
    strcpy(server_ip,getHostIP());
    server_port = atoi(argv[1]);
    mylisten();

    return 0;
}
