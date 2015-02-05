
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
#include <stdio.h>

#include <stdbool.h>
#include <sys/socket.h>
#include <poll.h>


#include <time.h>
#include <pthread.h>

#include "iphelper.h"
#include "sockethelper.h"


#define MAXBUFLEN 2048
#define MAX_LISTEN_SOCKETS 10


struct test_config{
    
    struct sockaddr_storage remoteAddr;
    struct sockaddr_storage localAddr;
    int sockfd;
    int icmpSocket;

    int numSentPkts;
    int numRcvdPkts;
    int numSentProbe;
    int numRcvdICMP;
};



static void *sendData(struct test_config *config)
{
    struct timespec timer;
    struct timespec remaining;

    timer.tv_sec = 0;
    timer.tv_nsec = 50000000;
    
    for(;;) {
        nanosleep(&timer, &remaining);
        config->numSentPkts++;
        printf("\rSent packet: (%i)", config->numSentPkts);
        fflush(stdout);
        sendPacket(config->sockfd,
                       (uint8_t *)"ICMP_Test",
                       9,
                       (struct sockaddr *)&config->remoteAddr,
                       false,
                       0);
    }
}

static void *socketListen(void *ptr){
    struct pollfd ufds[MAX_LISTEN_SOCKETS];
    struct test_config *config = (struct test_config *)ptr;
    struct sockaddr_storage their_addr;
    unsigned char buf[MAXBUFLEN];
    socklen_t addr_len;
    int rv;
    int numbytes;
    int i;
    int numSockets = 0;
    int keyLen = 16;
    char md5[keyLen];



    //Normal send/recieve RTP socket..
    ufds[0].fd = config->sockfd;
    ufds[0].events = POLLIN | POLLERR;
    numSockets++;

    //Listen on the ICMP socket if it exists
    if(config->icmpSocket != 0){
        ufds[1].fd = config->icmpSocket;
        ufds[1].events = POLLIN | POLLERR;
        numSockets++;
    }

   
    addr_len = sizeof their_addr;
    
    while(1){
        rv = poll(ufds, numSockets, -1);
        if (rv == -1) {
            perror("poll"); // error occurred in poll()
        } else if (rv == 0) {
            printf("Timeout occurred! (Should not happen)\n");
        } else {
            // check for events on s1:
            for(i=0;i<numSockets;i++){
                if (ufds[i].revents & POLLIN) {
                    if(i == 0){
                        if ((numbytes = recvfrom(config->sockfd, buf, 
                                                 MAXBUFLEN , 0, 
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                            perror("recvfrom");
                            exit(1);
                        }
                        config->numRcvdPkts++;
                        printf("\r \033[18C Recieved packet: (%i) ",config->numRcvdPkts);
                    }
                    if(i == 1){
                        if ((numbytes = recvfrom(config->icmpSocket, buf, 
                                                 MAXBUFLEN , 0, 
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                            perror("recvfrom");
                            exit(1);
                        }
                        config->numRcvdICMP++;
                        printf("\r \033[75C Recieved ICMP: (%i) ",config->numRcvdICMP);
                    }
                }
            }
        }
    }
}



int main(int argc, char **argv)
{
    struct test_config config;
    
    pthread_t sendDataThread;
    pthread_t listenThread;

    memset(&config, 0, sizeof(config));

    if(!getRemoteIpAddr((struct sockaddr *)&config.remoteAddr, 
                            argv[2], 
                        3478)){
        printf("Error getting remote IPaddr");
        exit(1);
        }
    
    if(!getLocalInterFaceAddrs( (struct sockaddr *)&config.localAddr, 
                                argv[1], 
                                config.remoteAddr.ss_family, 
                                IPv6_ADDR_NORMAL, 
                                false)){
        printf("Error local getting IPaddr on %s\n", argv[1]);
        exit(1);
    }
    /* Setting up UDP socket and a ICMP sockhandle */

    config.sockfd = createLocalUDPSocket(config.remoteAddr.ss_family, (struct sockaddr *)&config.localAddr, 0);

    if(config.remoteAddr.ss_family == AF_INET)
        config.icmpSocket=socket(config.remoteAddr.ss_family, SOCK_DGRAM, IPPROTO_ICMP);
    else
        config.icmpSocket=socket(config.remoteAddr.ss_family, SOCK_DGRAM, IPPROTO_ICMPV6);

    if (config.icmpSocket < 0) {
        perror("socket");
        exit(1);
    }
    
    //Start and listen to the sockets.
    pthread_create(&listenThread, NULL, (void *)socketListen, &config);
    
    //Start a thread that sends packet to the destination (Simulate RTP)
    pthread_create(&sendDataThread, NULL, (void *)sendData, &config);
    
    sleep(1);
    //Send a probe ackets with increasing TTLs
    for(int i=1;i<30;i++){
        config.numSentProbe++;
        printf("\r \033[45C Probe sent: (%i) (ttl:%i)", config.numSentProbe, i);
        fflush(stdout);
        sendPacket(config.sockfd,
                   (uint8_t *)"ICMP_Test_TTL",
                   13,
                   (struct sockaddr *)&config.remoteAddr,
                   false,
                   i);
        sleep(2);
    }
        
    sleep(5);

    //done
}
