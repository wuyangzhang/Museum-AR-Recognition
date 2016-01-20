/*************************************************
#
# Purpose: main program for "CPS server"
# Author.: Zihong Zheng (zzhonzi@gmail.com)
# Version: 0.1
# License: 
#
*************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/time.h>
#include <queue>
#include <getopt.h>
#include <unordered_map>
#include <errno.h>
#include <sys/stat.h>
#include <thread>

#include "ImgMatch.h"
#include "MsgDistributor.h"
#include "KafkaProducer.h"
#include "Metrics.h"
#include "MFPackager.h"
#include <popt.h>
#include <oml2/omlc.h>

#define USE_OPTS /* Include command line parsing code*/
#include "virtual-server_popt.h"

#define OML_FROM_MAIN  /* Define storage for some global variables; #define this in only one file */
#include "virtual-server_oml.h"

#include "config.h"
#include "aspgenerator.h"
#include "global_config.h"
/******************************************************************************
Description.: Display a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " \n" \
            " Help for server-OpenCV application\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this software:\n\n" \
            " [-h | --help ]........: display this help\n" \
            " [-v | --version ].....: display version information\n"
            " [-orbit]..............: run in orbit mode\n" \
            " [-d]..................: debug mode, print more details\n" \
            " [-m]..................: mine GUID, a.k.a src_GUID\n" \
            " [-o]..................: other's GUID, a.k.a dst_GUID\n" \
            " [-storm]..............: run in storm mode\n" \
            " [-train]..............: train with the database\n" \
            " [-p]..................: parallelism level (default 5)\n" \
            " [-kafka]..................: using kafka to submit works\n" \
            " [-mf]..................: new orbit mode\n" \
            " \n" \
            " ---------------------------------------------------------------\n" \
            " Please start the server first\n"
            " If you have not already train the Img database, use train option to train it\n"
            " Your need to be the root or use sudo to run the orbit mode\n"
            " Last, you should run the mfstack first before run this application in orbit mode\n"
            " ---------------------------------------------------------------\n" \
            " sample commands:\n" \
            "   # train with the database, create the yml file only\n" \
            "   ./server-OpenCV -train\n\n" \
            "   # start the server, run in debug mode\n" \
            "   ./server-OpenCV -d\n\n" \
            "   # start the server with STORM, all requests would be passed to storm framework\n" \
            "   ./server-OpenCV -storm\n\n" \
            "   # run in orbit mode, using MF network, set myGUID as 102, otherGUID as 101\n" \
            "   sudo ./server-OpenCV -orbit -m 102 -o 101\n" \
            " \n");
}

/******************************************************************************
Description.: print out the error message and exit
Input Value.:
Return Value:
******************************************************************************/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/******************************************************************************
Description.: print out the error message and exit
Input Value.:
Return Value:
******************************************************************************/
void errorSocket(const char *msg, int sock)
{
    errno = EBADF;
    perror(msg);
    printf("[server] Connection closed. --- error\n\n");
    if (tcp)
    {
        close(sock);     
        pthread_exit(NULL); //terminate calling thread!
    }
    else
    {
        // MsgD.close(sock, 1);
    }
}


/******************************************************************************
Description.: this is the transmit child thread
              it is responsible to send out one frame
Input Value.:
Return Value:
******************************************************************************/
void *result_child(void *arg)
{
    if (debug) printf("result child thread\n");

    struct arg_result *args = (struct arg_result *)arg;
    int sock = args->sock;
    char *file_name = args->file_name;
    int matchedIndex;
    char defMsg[] = "none";
    char sendInfo[200];
    vector<float> coord;
    ImgMatch imgM;

    // start matching the image
    imgM.matchImg(file_name);
    matchedIndex = imgM.getMatchedImgIndex();
    if (matchedIndex == 0) 
    {   
        // write none to client
        if (tcp) {
            if (write(sock, defMsg, sizeof(defMsg)) < 0)
            {
               errorSocket("ERROR writting to socket", sock);
            }
            printf("Not match.\n\n");

        }
        else if (orbit)
        {
            MsgD.send(sock, defMsg, sizeof(defMsg));
            printf("Not match.\n\n");
        }
        else if (mf) {
            mfpack->sendResult(defMsg, sizeof(defMsg));
            printf("Not match.\n\n");
        }
    }
    else
    {
        // send result to client
        // coord = imgM.calLocation();
        string info = imgM.getInfo();
        // sprintf(sendInfo, "%s,%d,%f,%f,%f,%f,%f,%f,%f,%f", info.c_str(), matchedIndex, coord.at(0), coord.at(1), coord.at(2), coord.at(3), coord.at(4), coord.at(5), coord.at(6), coord.at(7));
        sprintf(sendInfo, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,", info.c_str(), matchedIndex, 0, 0, 0, 0, 0, 0, 0, 0);
        printf("Matched Index: %d\n\n", matchedIndex);

        if (debug) printf("sendInfo: %s\n", sendInfo);
        if (tcp)
        {
            if (write(sock, sendInfo, sizeof(sendInfo)) < 0)
            {
                errorSocket("ERROR writting to socket", sock);
            }
        }
        else if (orbit)
        {
            MsgD.send(sock, sendInfo, sizeof(sendInfo));
        }
        else if (mf) {
            mfpack->sendResult(sendInfo, sizeof(sendInfo));
        }
    }

    // handle the metrics
    metrics->finish_request();

    if (debug) printf("------------- end matching -------------\n");

    // now release process_sem
    sem_post(&process_sem);
    return NULL;

}

/******************************************************************************
Description: function for sending back the result
Input Value.:
Return Value:
******************************************************************************/
void server_result (int sock, string userID)
{
    if (debug) printf("result thread\n\n");

    int ret, fd;
    char response[] = "ok";
    char buf[1024];
    sem_t *sem_match = new sem_t(); // create a new semaphore in heap
    queue<string> *imgQueue = 0;    // queue storing the file names

    //  Init semaphore and put the address of semaphore into map
    if (sem_init(sem_match, 0, 0) != 0)
    {
        errorSocket("ERROR semaphore init failed", sock);
    }
    // grap the lock
    pthread_mutex_lock(&sem_map_lock);
    sem_map[userID] = sem_match;
    pthread_mutex_unlock(&sem_map_lock);

    // reponse to the client
    if (tcp)
    {
        ret = write(sock, response, sizeof(response));
        if (ret < 0)
        {
            error("ERROR writting to socket");
        }
    }
    else if (orbit)
    {
        MsgD.send(sock, response, sizeof(response));
    }
    // new orbit mode need not to response
    
    if (storm)
    {
        // UDP setting for receiving result from storm
        struct sockaddr_in myaddr;
        int serverPort = 9879;

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            printf("socket create failed\n");
        if (debug) printf("socket created\n");

        /* bind it to all local addresses and pick any port number */

        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(serverPort);

        if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            perror("bind failed");
            goto stop;
        }       
        if (debug) printf("socket binded\n");
    }

    while(!global_stop) 
    {
        sem_wait(sem_match);

        // get the address of image queue
        if (imgQueue == 0)
        {
            imgQueue = queue_map[userID];
        }

        // check if the queue is empty
        if (imgQueue->empty())
        {
            sem_map.erase(userID);
            queue_map.erase(userID);
            user_map.erase(userID);
            delete(sem_match);
            delete(imgQueue);
            sem_destroy(sem_match);
            printf("[server] client disconnected --- result\n");
            return;
        }

        if (!storm)
        {
            // first try to grab process_sem
            sem_wait(&process_sem);

            if (debug) printf("\n----------- start matching -------------\n");
            string file_name = imgQueue->front(); 
            if (debug) printf("file name: [%s]\n", file_name.c_str());
            imgQueue->pop();

            // create a new thread to do the image processing

            pthread_t thread_id;
            struct arg_result trans_info;
            trans_info.sock = sock;
            strcpy(trans_info.file_name, file_name.c_str());
            /* create thread and pass socket and file name to send file */
            if (pthread_create(&thread_id, 0, result_child, (void *)&(trans_info)) == -1)
            {
                fprintf(stderr,"pthread_create error!\n");
                break; //break while loop
            }
            pthread_detach(thread_id);            
        }
        else
        {
            imgQueue->pop();

            // receive part
            bzero(buf, sizeof(buf));
            printf("wait for the result...\n");
            ret = recv(fd, buf, sizeof(buf), 0);
            if (ret < 0)
            {
                printf("receive error\n");
            }
            else
            {
                // int matchedIndex = atoi(buf);
                string numbers(buf);
                string indexString = strtok(buf, ",");
                // cout << numbers << endl;
                int matchedIndex = stoi(indexString);

                printf("received result: %d\n\n", matchedIndex);
                char defMsg[] = "none";
                char sendInfo[1024];
                if (matchedIndex == 0)
                {
                    // write none to client
                    if (tcp)
                    {
                        if (write(sock, defMsg, sizeof(defMsg)) < 0)
                        {
                           errorSocket("ERROR writting to socket", sock);
                        }
                    }
                    else if (orbit)
                    {
                        MsgD.send(sock, defMsg, sizeof(defMsg));
                    }
                    else if (mf) {
                        mfpack->sendResult(defMsg, sizeof(defMsg));
                    }

                    if (debug) printf("not match\n");
                }
                else
                {
                    // send result to client
                    string info = ImgMatch::getInfo(matchedIndex);
                    sprintf(sendInfo, "%s,%s", info.c_str(), numbers.c_str());
                    
                    if (debug) printf("sendInfo: %s\n", sendInfo);

                    if (tcp)
                    {
                        if (write(sock, sendInfo, sizeof(sendInfo)) < 0)
                        {
                            errorSocket("ERROR writting to socket", sock);
                        }
                    }
                    else if (orbit)
                    {
                        MsgD.send(sock, sendInfo, sizeof(sendInfo));
                    }
                    else if (mf) {
                        mfpack->sendResult(sendInfo, sizeof(sendInfo));
                    }

                    if (debug) printf("matched image index: %d\n", matchedIndex);
                }

            }

            // handle the metrics
            metrics->finish_request();
        }

        // end
    }

    stop:
    if (tcp)
    {
        close(sock);
    }
    if (storm) {
        close(fd);
    }
    printf("[server] Connection closed. --- result\n\n");
    delete(sem_match);
    // pthread_exit(NULL); //terminate calling thread!
    return;

}

/******************************************************************************
Description: function for transmitting the frames
Input Value.:
Return Value:
******************************************************************************/
void server_transmit (int sock, string userID)
{
    // printf("transmitting part\n");

    int n;
    char response[] = "ok";
    char file_name_temp[60];
    char *file_name;
    char *file_size_char;
    int file_size;
    int length = 0;
    int fake_index = 1;
    queue<string> *imgQueue = new queue<string>();    // queue storing the file names 

    // grap the lock
    pthread_mutex_lock(&queue_map_lock);
    queue_map[userID] = imgQueue; // put the address of queue into map
    pthread_mutex_unlock(&queue_map_lock);

    pthread_mutex_t queueLock; // mutex lock for queue operation
    sem_t *sem_match = 0;

    // init the mutex lock
    if (pthread_mutex_init(&queueLock, NULL) != 0) {
        errorSocket("ERROR mutex init failed", sock);
    }

    if (tcp) {
        // reponse to the client
        n = write(sock, response, sizeof(response));
        if (n < 0) {
            pthread_mutex_destroy(&queueLock);
            errorSocket("ERROR writting to socket", sock);
        }
    }
    else if (orbit) {
        // reponse to the client
        MsgD.send(sock, response, sizeof(response));
    }

    while (!global_stop)
    {
        if (debug) printf("wait for new request\n");
        char* img;

        // calculate the time consumption here
        struct timeval tpstart,tpend;
        double timeuse;

        if (tcp) {
            char buffer[BUFFER_SIZE];
            // receive the file info
            bzero(buffer, sizeof(buffer));
            n = read(sock, buffer, sizeof(buffer));
            if (n <= 0)
            {
                pthread_mutex_destroy(&queueLock);
                // signal the result thread to terminate
                sem_post(sem_match);
                errorSocket("ERROR reading from socket", sock);
            } 

            gettimeofday(&tpstart,NULL);

            if (debug) printf("message: %s\n", buffer);
            if (debug) printf("split the message\n");
            // store the file name and the block count
            file_name = strtok(buffer, ",");
            strcpy(file_name_temp, file_name);
            if (debug) printf("\n[server] file name: [%s]\n", file_name);
            file_size_char = strtok(NULL, ",");
            file_size = strtol(file_size_char, NULL, 10);
            if (debug) printf("file size: %d\n", file_size);
            // reponse to the client
            n = write(sock, response, sizeof(response));
            if (n <= 0)
            {
                pthread_mutex_destroy(&queueLock);
                // signal the result thread to terminate
                sem_post(sem_match);
                errorSocket("ERROR writting to socket", sock);
            }

            // receive the data from client and store them into buffer
            img = new char[file_size];
            int offset = 0;
            int done = 0;
            // receive the data from server and store them into buffer
            bzero(buffer, sizeof(buffer));
            // int num = 1;
            while((length = recv(sock, buffer, sizeof(buffer), 0)))  
            {
                if (length < 0)  
                {  
                    printf("Recieve Data From Client Failed!\n");  
                    break;  
                }
                // printf("num: %d, len: %d\n", num++, length);

                if (offset + length == file_size) {
                    done = 1;
                }
          
                // copy the content into img
                for (int i = 0; i < length; ++i)
                {
                    img[i + offset] = buffer[i];
                }

                bzero(buffer, sizeof(buffer));
                if (done)
                {
                    if (debug) printf("file size full\n");
                    break;
                }
                offset += length;
            }

            // reponse to the client
            write(sock, response, sizeof(response));
        }
        else if (orbit) {
            // get the id length
            int id_length = 1;
            int divisor = 10;
            while (sock / divisor > 0)
            {
                ++id_length;
                divisor *= 10;
            }
            int recv_length = BUFFER_SIZE * 4; // 4096 bytes per time
            char buffer[recv_length];
            int received_size = 0;

            bzero(buffer, sizeof(buffer));
            // get the file info from client
            n = MsgD.recv(sock, buffer, 100);
            if (n < 0)
            {
                pthread_mutex_destroy(&queueLock);
                // signal the result thread to terminate
                sem_post(sem_match);
                errorSocket("ERROR reading from socket", sock);
                return;
            } 
            file_name = strtok(buffer, ",");
            strcpy(file_name_temp, file_name);
            printf("\n[server] file name: [%s]\n", file_name);
            file_size_char = strtok(NULL, ",");
            file_size = strtol(file_size_char, NULL, 10);
            printf("[server] file size: %d\n", file_size);

            // handle the metrics
            metrics->submit_request();

            // calculate the time consumption here
            gettimeofday(&tpstart,NULL);

            // reponse to the client
            MsgD.send(sock, response, sizeof(response));

            img = new char[file_size];
            int offset = 0;
            // receive the data from client and store them into buffer
            while(1)  
            {
                bzero(buffer, sizeof(buffer));
                n = MsgD.recv(sock, buffer, sizeof(buffer));
                if (n <= 0)
                {
                    pthread_mutex_destroy(&queueLock);
                    // signal the result thread to terminate
                    sem_post(sem_match);
                    errorSocket("ERROR reading from socket", sock);
                } 
                
                if (file_size - received_size <= recv_length)
                {
                    int remain = file_size - received_size;
                    // copy the content into img
                    for (int i = 0; i < remain; ++i)
                    {
                        img[i + offset] = buffer[i];
                    }
                    break;
                }

                // copy the content into img
                for (int i = 0; i < recv_length; ++i)
                {
                    img[i + offset] = buffer[i];
                }
                offset += recv_length;

                received_size += recv_length;
            }
        }
        else if (mf) {
            img = new char[650000]; // large enough to hold the image file
            file_size = mfpack->recvImage(img, 650000);
            bzero(file_name_temp, sizeof(file_name_temp));
            sprintf(file_name_temp, "pics/%d.jpg", fake_index++);
            printf("\n[server] file name: [%s]\n", file_name_temp);
            printf("[server] file size: %d\n", file_size);

            gettimeofday(&tpstart,NULL);
        }

        // print out time comsumption
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;// notice, should include both s and us
        // printf("used time:%fus\n",timeuse);
        printf("receive used time:%fms\n",timeuse / 1000);
        // finished
        if (debug) printf("[server] Recieve Finished!\n\n");

        // handle the metrics
        metrics->submit_request();

        // if not using storm, write into disk
        if (!storm)
        {
            FILE *fp = fopen(file_name_temp, "w");  
            if (fp == NULL)  
            {  
                printf("File:\t%s Can Not Open To Write!\n", file_name_temp);  
                break;
            }
            fwrite(img, sizeof(char), file_size, fp);
            fclose(fp);
        }
        // if using storm mode, transfer the file
        else if (storm) {
            // using kafka to pass the file
            if (kafka) {
                // string input = string(file_name_temp);
                // producer->sendString(input, input.size());

                printf("Current metrics: %f\n", metrics->get_metrics());
                printf("Now submit to Kafka.\n");
                producer->send(img, file_size);
            }
            // use tcp socket to pass the file
            else {
                // send request to spout
                if (debug) printf("Now try to connect the spout\n");
                int sockfd, ret;
                int spoutPort = 9878;
                struct sockaddr_in spout_addr;
                struct hostent *spout;
                struct in_addr ipv4addr;
                char buf_spout[100];
                char* spout_IP;
                const int len = spoutIP.length();
                spout_IP = new char[len+1];
                strcpy(spout_IP, spoutIP.c_str());

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0)
                {
                    printf("ERROR opening socket\n");
                    return;
                }
                inet_pton(AF_INET, spout_IP, &ipv4addr);
                spout = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);
                if (debug) printf("\n[server] Spout address: %s\n", spout_IP);
                if (spout == NULL) {
                    fprintf(stderr,"ERROR, no such host\n");
                    exit(0);
                }
                bzero((char *) &spout_addr, sizeof(spout_addr));
                spout_addr.sin_family = AF_INET;
                bcopy((char *)spout->h_addr, (char *)&spout_addr.sin_addr.s_addr, spout->h_length); 
                spout_addr.sin_port = htons(spoutPort);

                while (connect(sockfd,(struct sockaddr *) &spout_addr, sizeof(spout_addr)) < 0)
                {
                    printf("The spout is not available now, wait a while and reconnect\n\n");
                    usleep(100000); // sleep 100ms
                }

                printf("[server] Get connection to spout\n");

                bzero(buf_spout, sizeof(buf_spout));
                sprintf(buf_spout, "%d", file_size);
                if (debug) printf("[server] send the file size\n");
                ret = write(sockfd, buf_spout, sizeof(buf_spout));
                if (ret < 0)
                {
                    printf("error sending\n");
                    return;
                }

                // get the response
                bzero(buf_spout, sizeof(buf_spout));
                if (debug) printf("[server] now wait for response\n");
                ret = read(sockfd, buf_spout, sizeof(buf_spout));
                if (ret < 0)
                {
                    printf("error reading\n");
                    return;
                }

                if (debug) printf("got response: %s\n", buf_spout);

                if (debug) printf("[server] send the img\n");
                ret = write(sockfd, img, file_size);
                if (ret < 0)
                {
                    printf("error sending\n");
                    return;
                }
                if (debug) printf("ret: %d\n", ret);

                // get the ack
                read(sockfd, buf_spout, sizeof(buf_spout));
                printf("[server] Finished transmitting image to spout\n\n");

                close(sockfd);
            }

            delete[] img;
        }

        // lock the queue, ensure there is only one thread modifying the queue
        pthread_mutex_lock(&queueLock);

        // store the file name to the waiting queue
        string file_name_string(file_name_temp);
        imgQueue->push(file_name_string);

        pthread_mutex_unlock(&queueLock);
        // get the address of sem_match
        if (sem_match == 0)
        {
            while (sem_map.find(userID) == sem_map.end());
            sem_match = sem_map[userID];
        }
        // signal the result thread to do image processing
        sem_post(sem_match);
    }

    if (tcp)
       close(sock);
    
    delete(imgQueue);
    printf("[server] Connection closed. --- transmit\n\n");
    return;

}

void *mfThread(void* inputtype) {
    user_map["mf"] = 2;
    int type = (int)inputtype;
    if (type == 0) {
        server_result(0, "mf");
    }
    else if (type == 1) {
        server_transmit(0, "mf");
    }

    return 0;
}

/******************************************************************************
Description.: There is a separate instance of this function 
              for each connection.  It handles all communication
              once a connnection has been established.
Input Value.:
Return Value: -
******************************************************************************/
void *serverThread(void* inputsock)
{
    int sock = (int)inputsock;
    int n;
    char buffer[100];
    string userID;
    char *threadType;
    char fail[] = "failed";

    // Receive the header
    bzero(buffer, sizeof(buffer));
    if (tcp)
    {
        n = read(sock, buffer, sizeof(buffer));
        if (n < 0)
        {
            errorSocket("ERROR reading from socket", sock);
        } 
    }
    // below is orbit mode, using MFAPI
    else if (orbit)
    {
        MsgD.recv(sock, buffer, sizeof(buffer));
    }

    printf("[server] header content: %s\n\n",buffer);

    threadType = strtok(buffer, ",");
    userID = strtok(NULL, ",");

    // grap the lock
    pthread_mutex_lock(&user_map_lock);
    // confirm that this user does not log in
    if (user_map.find(userID) == user_map.end())
    {
        // put the new user into user map
        user_map[userID] = 1;
    }
    else
    {
        if (user_map[userID] == 1)
        {
            // increase user thread count
            user_map[userID] = 2;
        }
        else
        {
            // remember to unlock!
            pthread_mutex_unlock(&user_map_lock);
            // reponse to the client
            if (tcp)
            {
                if (write(sock, "failed", sizeof("failed")) < 0)
                {
                    errorSocket("ERROR writting to socket", sock);
                }
                close(sock); 
            }
            else
            {
                MsgD.send(sock, fail, sizeof(fail));
            }
            printf("[server] User exist. Connection closed.\n\n");
            return 0;
        }
    }
    pthread_mutex_unlock(&user_map_lock);

    if (strcmp(threadType, "transmit") == 0) 
    {
        server_transmit(sock, userID);
    }
    else if (strcmp(threadType, "result") == 0) 
    {
        server_result(sock, userID);
    }
    else
    {
        if (tcp)
        {
            close(sock); 
        }
        printf("[server] Command Unknown. Connection closed.\n\n");
    }

    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the server threads
Input Value.: -
Return Value: -
******************************************************************************/
void server_main()
{
    printf("\n[server] start supporting service\n");

    if (tcp)
    {
        // init part
        int sockfd, newsockfd, portno;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        clilen = sizeof(cli_addr);

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            error("ERROR opening socket");
        } 
        else 
            if (debug) printf ("[server] obtain socket descriptor successfully.\n"); 
        bzero((char *) &serv_addr, sizeof(serv_addr));
        // set up the port number
        portno = PORT_NO;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        {
            error("ERROR on binding");
        }
        else
            if (debug) printf("[server] bind tcp port %d sucessfully.\n",portno);

        if(listen(sockfd,5))
        {
            error("ERROR listening");
        }
        else 
            if (debug) printf ("[server] listening the port %d sucessfully.\n\n", portno);    
        
        // init finished, now wait for a client
        while (!global_stop) {
            pthread_t thread_id;
            //Block here. Until server accpets a new connection.
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0)
            {
                // error("ERROR on accept");
                fprintf(stderr,"Accept error!\n");
                continue; //ignore current socket ,continue while loop.
            }
            else 
                printf ("[server] server has got connect from %s, socket id: %d.\n", (char *)inet_ntoa(cli_addr.sin_addr), newsockfd);

            /* create thread and pass context to thread function */
            if (pthread_create(&thread_id, 0, serverThread, (void *)newsockfd) == -1)
            {
                fprintf(stderr,"pthread_create error!\n");
                break; //break while loop
            }
            pthread_detach(thread_id);
            usleep(1000 * 5); //  sleep 5ms to avoid clients gain same sock

        } /* end of while */
        close(sockfd);
    }
    // below is orbit mode
    else if (orbit)
    {
        int newsockfd;   
        // init finished, now wait for a client
        while (!global_stop) {
            pthread_t thread_id;
            //Block here. Until server accpets a new connection.
            newsockfd = MsgD.accept();
            if (newsockfd < 0)
            {
                // error("ERROR on accept");
                fprintf(stderr,"Accept error!\n");
                continue; //ignore current socket ,continue while loop.
            }
            else 
                printf ("[server] server has got new connection, socket id: %d.\n", newsockfd);

            /* create thread and pass context to thread function */
            if (pthread_create(&thread_id, 0, serverThread, (void *)newsockfd) == -1)
            {
                fprintf(stderr,"pthread_create error!\n");
                break; //break while loop
            }
            pthread_detach(thread_id);
            usleep(1000 * 5); //  sleep 5ms to avoid clients gain same sock

        } /* end of while */
    }
    // mf virtual network 
    else if (mf) {
        // start transmit and result threads
        pthread_t thread1, thread2;
        int type = 0;
        if (pthread_create(&thread1, 0, mfThread, (void *)type) == -1)
        {
            fprintf(stderr,"pthread_create error!\n");
        }
        pthread_detach(thread1);

        type = 1;
        if (pthread_create(&thread2, 0, mfThread, (void *)type) == -1)
        {
            fprintf(stderr,"pthread_create error!\n");
        }
        pthread_detach(thread2);
        //start generating asp 
        aspGenerator.start();
        pause();
    }

    if (debug) printf("main end\n");
}

/******************************************************************************
Description.: this is the mflisten thread
              it loops forever, listen on the src_GUID
Input Value.:
Return Value:
******************************************************************************/
void *mflisten_thread(void *arg)
{
    if (debug) printf("\nin listen thread\n");
    while (!global_stop) {
        MsgD.listen();
    }

    if (debug) printf("mflisten end\n");
    return NULL;
}

/******************************************************************************
Description.: calling this function start the server and listener
Input Value.: -
Return Value: -
******************************************************************************/
void server_run()
{
    metrics = new Metrics();

    if (kafka) {
        producer = new KafkaProducer();
    }

    if (storm && !kafka)
    {
        // try to get the IP of spout
        struct sockaddr_in myaddr, remaddr;
        int fd, slen = sizeof(remaddr);
        socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
        char spoutFinderHost[100];
        sprintf(spoutFinderHost, "127.0.0.1");  /* change this to use a different server */
        char buf[1024];
        int ret;            /* # bytes received */
        int spoutFinderPort = 9877;
        // int spoutPort = 9878;
        int serverPort = 9879;

        /* create a socket */

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            printf("socket create failed\n");
        if (debug) printf("socket created\n");

        /* bind it to all local addresses and pick any port number */

        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(serverPort);

        if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            perror("bind failed");
            return;
        }       
        if (debug) printf("socket binded\n");

        /* now define remaddr, the address to whom we want to send messages */
        /* For convenience, the host address is expressed as a numeric IP address */
        /* that we will convert to a binary format via inet_aton */

        memset((char *) &remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port = htons(spoutFinderPort);
        if (inet_aton(spoutFinderHost, &remaddr.sin_addr) == 0) {
            fprintf(stderr, "inet_aton() failed\n");
            exit(1);
        }

        /* now let's send the messages */

        if (debug) printf("Sending packet to %s port %d\n", spoutFinderHost, spoutFinderPort);
        sprintf(buf, "where");
        if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen) == -1)
        {
            perror("sendto");
        }

        // now receive the IP of spout
        bzero(buf, sizeof(buf));
        ret = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&remaddr, &addrlen);
        if (ret > 0)
        {
            if (strcmp(buf, "none") == 0)
            {
                printf("The spout is not ready yet.\n");
                close(fd);
                return;   
            }
            else {
                string spoutIP_temp(buf);
                spoutIP = spoutIP_temp;
                printf("Spout IP: %s\n", spoutIP.c_str());
            }
        }
        else
        {
            printf("receive error\n");      
        }
        close(fd);
    }

    if (!storm)
    {
        // prepare the img database
        ImgMatch::init_matchImg("./indexImgTable", "ImgIndex.yml", "/demo/info/");
    }

    if (orbit)
    {
        /* create thread and pass context to thread function */
        if (pthread_create(&mflistenThread, 0, mflisten_thread, NULL) == -1)
        {
            fprintf(stderr,"pthread_create error!\n");
            exit(1);
        }
        pthread_detach(mflistenThread);   
    }

    server_main();
}

/******************************************************************************
Description.: pressing CTRL+C sends signals to this process instead of just
              killing the threads can tidily shutdown and free allocated
              resources. The function prototype is defined by the system,
              because it is a callback function.
Input Value.: sig tells us which signal was received
Return Value: -
******************************************************************************/
void signal_handler(int sig)
{

    /* signal "stop" to threads */
    printf("\nSetting signal to stop.\n");
    global_stop = 1;
    if (kafka) {
        delete producer;
    }
    delete metrics;
    aspGenerator.stop();
    pthread_cancel(mflistenThread);
    pthread_mutex_destroy(&queue_map_lock);
    pthread_mutex_destroy(&sem_map_lock);
    pthread_mutex_destroy(&user_map_lock);
    sem_destroy(&process_sem);
    usleep(1000 * 1000);

    /* clean up threads */
    printf("Force cancellation of threads and cleanup resources.\n");

    usleep(1000 * 1000);

    printf("Done.\n");

    exit(0);
    return;
}


int main(int argc,const  char **argv)
{
    int src_GUID = -1, dst_GUID = -1, router_GUID = -1;
    int max_image = 5;
    double aspLoad = 0;
    int c,i,ret,opt;
    /* Reconstruct command line */
    size_t cmdline_len = 0;
    for(i = 0; i < argc; i++) {
      cmdline_len += strlen(argv[i]) + 1;
    }
    char cmdline[cmdline_len + 1];
    cmdline[0] = '\0';
    for(i = 0; i < argc; i++) {
      strncat(cmdline, argv[i], cmdline_len);
      cmdline_len -= strlen(argv[i]);
      strncat(cmdline, " ", cmdline_len);
      cmdline_len--;
    }

    /* Initialize OML */
    if((ret = omlc_init("vserver", &argc, argv, NULL)) < 0) {
      logerror("Could not initialise OML\n");
      return -1;
    }

    /* Parse command line arguments */
     poptContext optCon = poptGetContext(NULL, argc, argv, options, 0); /* options is defined in virtual-server_popt.h */
     while ((c = poptGetNextOpt(optCon)) > 0) {}

    /* Initialise measurement points and start OML */
    
     oml_register_mps(); /* Defined in virtual-server_oml.h */
    
    if(omlc_start()) {
      logerror("Could not start OML\n");
    return -1;
    }
   
    /* parameter parsing */
    while((opt = getopt(argc, (char **)argv, "hOdm:o:r:STp:KM1:")) != -1) {
        switch(opt)
        {
            /* h, help */
        case 'h':
            help();
            return 0;
            /* orbit, run in orbit mode */
        case 'O':
            orbit = true;
            tcp = false;
            break;

            /* debug mode */
        case 'd':
            debug = 1;
            break;

            /* mine GUID */
        case 'm':
            src_GUID = strtol(optarg, NULL, 10);
            break;

            /* other's GUID */
        case 'o':
            dst_GUID = strtol(optarg, NULL, 10);
            break;

            /* server's access router's GUID */
        case 'r':
            router_GUID = strtol(optarg, NULL, 10);
            break;

            /* storm mode */
        case 'S':
            storm = true;
            break;

            /* train mode */
        case 'T':
            train = true;
            break;

            /* parallelism level */
        case 'p':
            max_image = strtol(optarg, NULL, 10);
            break;

            /* kafka mode */
        case 'K':
            kafka = true;
            break;

            /* new orbit mode */
        case 'M':
            mf = true;
            tcp = false;
            break;
        case '1':

        default:
            help();
            return 0;
        }
    }

    if (tcp)
    {
        /* register signal handler for <CTRL>+C in order to clean up */
        if(signal(SIGINT, signal_handler) == SIG_ERR)
        {
            printf("could not register signal handler\n");
            exit(EXIT_FAILURE);
        }
    }

    // init the mutex lock
    if (pthread_mutex_init(&user_map_lock, NULL) != 0
        || pthread_mutex_init(&queue_map_lock, NULL) != 0
        || pthread_mutex_init(&sem_map_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    // init the semaphore
    if (sem_init(&process_sem, 0, max_image) != 0)
    {
        printf("semaphore init failed\n");
    }

    if (orbit || mf)
    {
        if (src_GUID != -1 && dst_GUID != -1)
        {
            if (debug) printf("src_GUID: %d, dst_GUID: %d\n", src_GUID, dst_GUID);
        }
        else
        {
            printf("ERROR: please enter src_GUID and dst_GUID with flags -m & -o\n");
            exit(1);
        }
        if (orbit) {
            /* init new Message Distributor */
            MsgD.init(src_GUID, dst_GUID, debug);
        }
        else if (mf) {
            /* ASR mf model */
            mfpack = new MFPackager(src_GUID, dst_GUID, router_GUID, debug);
	       
            aspGenerator.setMfHandle(mfpack->getMFHandle());
	        aspGenerator.setDestinationGuid(router_GUID);
            aspGenerator.setGuid(src_GUID);
            aspGenerator.init();
	        aspGenerator.setOmlMps(g_oml_mps_virtual_server);
	        aspGenerator.setCurrentLoad(aspLoad);
        }

    }

    if (train)
    {
        // now train with the database
        ImgMatch::init_DB(100,"/demo/img/","./indexImgTable","ImgIndex.yml");
        return 0;
    }

    server_run();

    return 0;
}

