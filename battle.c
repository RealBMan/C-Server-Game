#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 

#ifndef PORT
    #define PORT 53219
#endif

# define SECONDS 10
#define MAX_BUFF 256
struct client {
    int fd;
    char *name;
    struct in_addr ipaddr;
    struct client *next;
    int hitpoints;
    struct client *last_played;
    int in_match;
    int pm;
    int turn;
    int speak;
};
typedef struct {
    struct client *player1;
    struct client *player2;

} match;


static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top, int elapsed_time, time_t *start_time);
int playGame(struct client *c1, struct client *c2);
int startGame(struct client **match,struct client *top);
static void broadcast2(struct client *top, char *s, int size, struct client *p1); 


int bindandlisten(void);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;
    srand(time(NULL));

    time_t start_time,current_time;
    int elapsed_time;

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    start_time = time(NULL);

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            //continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            
            head = addclient(head, clientfd, q.sin_addr);
        
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        current_time = time(NULL);
                        elapsed_time = difftime(current_time,start_time);
                        int result = handleclient(p, head,elapsed_time, &start_time);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        } else if (result == -2){
                            int tmp_fd = p->last_played->fd;
                            head = removeclient(head, p->last_played->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd); 
                        }
                        break;
                    }
                }
            }
        }

        struct client *m;
        struct client *match[2];
        match[0] = NULL;
        match[1] = NULL;
        for(m = head; m != NULL; m = m->next){
            //printf("%s\n\n)", m->name);
            if(!(m->in_match)&& m->name != NULL){
                if(match[0] == NULL){
                    match[0] = m;
                } else if(m->last_played != match[0]){
                    match[1] = m;
                    match[0]->in_match = 1;
                    match[1]->in_match = 1;
                    int check = startGame(match,head);
                    if (check > 0){
                        int tmp_fd = check;
                        head = removeclient(head, p->fd);
                        FD_CLR(tmp_fd, &allset);
                        close(tmp_fd);
                    }
                    match[0] = NULL;
                    match[1] = NULL;
                }
            }
        }
        
    }
    return 0;
}

int startGame(struct client **match, struct client *top){
    char buff[MAX_BUFF];
    char msg7[512];
    char msg8[512] = "Awaiting next opponent...\r\n\n";
    char outbuf[512];
    //char buff2[MAX_BUFF];
    int len;
    char msg1[MAX_BUFF] = "(a)ttack\n(p)owermove\n(s)peak something\r\n";
    sprintf(buff, "You engage %s!\r\n", match[0]->name);
    len = write(match[1]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
        if(match[1]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[1]->fd;
    }
    sprintf(buff, "You engage %s!\r\n", match[1]->name);
    len = write(match[0]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
        if(match[0]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[0]->fd;
    }

    match[0]->in_match = 1;
    match[1]->in_match = 1;
    match[0]->last_played = match[1];
    match[1]->last_played = match[0];

    match[0]->hitpoints = 20 + rand() % 11;
    match[1]->hitpoints = 20 + rand() % 11;

    match[0]->pm = 1 + rand() % 3;
    match[1]->pm = 1 + rand() % 3;

    int choice = 0 + rand() % 2;
    
    sprintf(buff, "Your hitpoints: %d\r\n", match[0]->hitpoints);
    len = write(match[0]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
        if(match[0]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[0]->fd;
    }
    sprintf(buff, "Your hitpoints: %d\r\n", match[1]->hitpoints);
    len = write(match[1]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
        if(match[1]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[1]->fd;
    }

    sprintf(buff, "Your powermoves: %d\r\n\n", match[0]->pm);
    len = write(match[0]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
        if(match[0]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[0]->fd;
    }
    sprintf(buff, "Your powermoves: %d\r\n\n", match[1]->pm);
    len = write(match[1]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
        if(match[1]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[1]->fd;
    }

    sprintf(buff, "%s's hitpoints: %d\r\n\n", match[1]->name, match[1]->hitpoints);
    len = write(match[0]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
        if(match[0]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[0]->fd;
    }
    sprintf(buff, "%s's hitpoints: %d\r\n\n", match[0]->name, match[0]->hitpoints);
    len = write(match[1]->fd, buff, strlen(buff));
    if (len <= 0){
        sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
        printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
        if(match[1]->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            write(match[0]->fd, msg7, strlen(msg7));
            write(match[0]->fd, msg8, strlen(msg8));
           
            match[1]->in_match = 0;
            match[1]->speak = 0;
            match[1]->turn = 0;
            match[1]->last_played = NULL;

            match[0]->in_match = 0;
            match[0]->speak = 0;
            match[0]->turn = 0;
            match[0]->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
        broadcast(top, outbuf, strlen(outbuf));
        return match[1]->fd;
    }
    if(choice){
        match[choice]->turn = 1;
        match[0]->turn = 0;
        sprintf(buff, "Waiting for %s to strike...\r\n", match[1]->name);
        len = write(match[0]->fd, buff, strlen(buff));
        if (len <= 0){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
            if(match[0]->in_match){
                sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
                write(match[0]->fd, msg7, strlen(msg7));
                write(match[0]->fd, msg8, strlen(msg8));
           
                match[1]->in_match = 0;
                match[1]->speak = 0;
                match[1]->turn = 0;
                match[1]->last_played = NULL;

                match[0]->in_match = 0;
                match[0]->speak = 0;
                match[0]->turn = 0;
                match[0]->last_played = NULL;
            }
            sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
            broadcast(top, outbuf, strlen(outbuf));
            return match[0]->fd;
        }
        len = write(match[choice]->fd, msg1, strlen(msg1));
        if (len <= 0){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
            if(match[1]->in_match){
                sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
                write(match[0]->fd, msg7, strlen(msg7));
                write(match[0]->fd, msg8, strlen(msg8));

                match[1]->in_match = 0;
                match[1]->speak = 0;
                match[1]->turn = 0;
                match[1]->last_played = NULL;

                match[0]->in_match = 0;
                match[0]->speak = 0;
                match[0]->turn = 0;
                match[0]->last_played = NULL;
            }
            sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
            broadcast(top, outbuf, strlen(outbuf));
            return match[1]->fd;
        }
    }else{
        match[choice]->turn = 0;
        match[0]->turn = 1;
        sprintf(buff, "Waiting for %s to strike...\r\n", match[0]->name);
        len = write(match[1]->fd, buff, strlen(buff));
        if (len <= 0){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
            printf("Disconnect from %s\n", inet_ntoa(match[1]->ipaddr));
            if(match[1]->in_match){
                sprintf(msg7, "--%s dropped. You win!\r\n\n",match[0]->name);
                write(match[0]->fd, msg7, strlen(msg7));
                write(match[0]->fd, msg8, strlen(msg8));
           
                match[1]->in_match = 0;
                match[1]->speak = 0;
                match[1]->turn = 0;
                match[1]->last_played = NULL;

                match[0]->in_match = 0;
                match[0]->speak = 0;
                match[0]->turn = 0;
                match[0]->last_played = NULL;
            }
            sprintf(outbuf, "**%s leaves**\r\n", match[1]->name);
            broadcast(top, outbuf, strlen(outbuf));
            return match[1]->fd;
        }

        len = write(match[0]->fd, msg1, strlen(msg1));
        if (len <= 0){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
            printf("Disconnect from %s\n", inet_ntoa(match[0]->ipaddr));
            if(match[0]->in_match){
                sprintf(msg7, "--%s dropped. You win!\r\n\n",match[1]->name);
                write(match[0]->fd, msg7, strlen(msg7));
                write(match[0]->fd, msg8, strlen(msg8));
           
                match[1]->in_match = 0;
                match[1]->speak = 0;
                match[1]->turn = 0;
                match[1]->last_played = NULL;

                match[0]->in_match = 0;
                match[0]->speak = 0;
                match[0]->turn = 0;
                match[0]->last_played = NULL;
            }
            sprintf(outbuf, "**%s leaves**\r\n", match[0]->name);
            broadcast(top, outbuf, strlen(outbuf));
            return match[0]->fd;
        }
    } 
    return 0;
}


int handleclient(struct client *p, struct client *top, int elapsed_time,time_t *start_time) {
    int len2;
    int len3;
    char msg1[MAX_BUFF] = "(a)ttack\n(p)owermove\n(s)peak something\r\n";
    char msg2[MAX_BUFF] = "(a)ttack\n(s)peak something\r\n";
    char msg3[MAX_BUFF] = "Speak: ";
    char msg4[MAX_BUFF] = "You speak: ";
    char msg5[512];
    char msg7[512];
    char msg8[512] = "Awaiting next opponent...\r\n\n";
    char msg6[MAX_BUFF] = "You win!\r\n";
    char buf[256];
    char outbuf[512];
    char buff[MAX_BUFF];

    int len = read(p->fd, buf, sizeof(buf) - 1);
    struct client *p2 = p->last_played;
    
    if (len > 0) {
        char *newline_ptr = strchr(buf, '\n');
        if (newline_ptr != NULL) {
            int index = newline_ptr - buf;
            buf[index] = '\0';
        }

        printf("Received %d bytes: %s", len, buf);
        printf("Time passed %d\n",elapsed_time);
        //sprintf(outbuf, "%s: %s says: %s", p->name, inet_ntoa(p->ipaddr), buf);
        //broadcast(top, outbuf, strlen(outbuf));

        if (p->name == NULL){
            p->name = malloc(sizeof(buf));
            strcpy(p->name, buf);

            char wlcm[50];
            int len2;
            sprintf(wlcm, "Welcome %s!\r\n", p->name);
            len2 = write(p->fd, wlcm, strlen(wlcm));
            if (len2 <= 0) {
                // socket is closed
                printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
                broadcast(top, outbuf, strlen(outbuf));
                return -1;
            }

            char welcome_msg[200];
            sprintf(welcome_msg, "**%s enters the arena**\r\n", p->name);
            broadcast2(top, welcome_msg, strlen(welcome_msg), p);
    
            char waiting[] = "Awaiting opponent...\r\n";
            len2 = write(p->fd, waiting, strlen(waiting));
            if (len2 <= 0) {
                // socket is closed
                printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
                broadcast(top, outbuf, strlen(outbuf));
                return -1;
            }
            return 0;
        } else if (p->in_match && p->turn){
            if (elapsed_time >= 15){
                sprintf(buff, "\r\n%s took too long to attack it is now your turn\r\n\n", p->name);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                sprintf(buff, "\r\n You took too long to attack\r\n\n");
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 

                p->turn = 0;
                p2->turn = 1;

            } else if(strcmp(buf,"a") == 0){
                printf("%s", buf);
                int attack = 2 + rand() % 5;
                p2->hitpoints = p2->hitpoints - attack;

                sprintf(buff, "\r\n%s hits you for %d damage!\r\n", p->name, attack);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                sprintf(buff, "\r\n You hit %s for %d damage!\r\n", p2->name, attack);
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
            
                p->turn = 0;
                p2->turn = 1;

            
            }else if(strcmp(buf,"p") == 0 && p->pm > 0){
                p->pm -= 1;
                int chance = 0 + rand() % 2;
                int attack = 3*(2 + rand() % 5);
                if(chance){
                    p2->hitpoints = p2->hitpoints - attack;

                    sprintf(buff, "\r\n%s powermoves you for %d damage!\r\n", p->name, attack);
                    len3 = write(p2->fd, buff, strlen(buff));
                    if (len3 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                        if(p2->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                            write(p->fd, msg7, strlen(msg7));
                            write(p->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -2;
                    } 

                    sprintf(buff, "\r\n You hit %s for %d damage!\r\n", p2->name, attack);
                    len2 = write(p->fd, buff, strlen(buff));
                    if (len2 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                        if(p->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                            write(p2->fd, msg7, strlen(msg7));
                            write(p2->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -1;
                    } 
                }else{
                    sprintf(buff, "%s missed you!\r\n", p->name);
                    len3 = write(p2->fd, buff, strlen(buff));
                    if (len3 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                        if(p2->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                            write(p->fd, msg7, strlen(msg7));
                            write(p->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -2;
                    } 

                    sprintf(buff, "You missed!\r\n\n");
                    len2 = write(p->fd, buff, strlen(buff));
                    if (len2 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                        if(p->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                            write(p2->fd, msg7, strlen(msg7));
                            write(p2->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -1;
                    } 
                }
                p->turn = 0;
                p2->turn = 1;
            }else if (strcmp(buf,"s") == 0){
                len2 = write(p->fd, msg3, strlen(msg3));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
                p->speak = 1;
            
            }else if (p->speak){
                len2 = write(p->fd, msg4, strlen(msg4));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
                len2 = write(p->fd, buf, strlen(buf));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
                sprintf(msg5,"%s took a break to tell you: %s\r\n", p->name, buf);
                len3 = write(p2->fd, msg5, strlen(msg5));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                }    

                p->speak = 0;
            
            }
            if(p2->hitpoints <= 0){
                sprintf(buff, "You are no match for %s. You scurry away...\r\n\n", p->name);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                len2 = write(p->fd, msg6, strlen(msg6));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 

                p->in_match = 0;
                p->speak = 0;
                p->turn = 0;

                p2->in_match = 0;
                p2->speak = 0;
                p->turn = 0;

                len3 = write(p2->fd, msg8, strlen(msg8));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                len2 = write(p->fd, msg8, strlen(msg8));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
            }else if(p2->turn){
                sprintf(buff, "Your hitpoints: %d\r\n", p2->hitpoints);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                sprintf(buff, "Your hitpoints: %d\r\n", p->hitpoints);
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 

                sprintf(buff, "Your powermoves: %d\r\n\n", p2->pm);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }   

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                sprintf(buff, "Your powermoves: %d\r\n\n", p->pm);
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                }    

                sprintf(buff, "%s's hitpoints: %d\r\n\n", p->name, p->hitpoints);
                len3 = write(p2->fd, buff, strlen(buff));
                if (len3 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                    if(p2->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                        write(p->fd, msg7, strlen(msg7));
                        write(p->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -2;
                } 

                sprintf(buff, "%s's hitpoints: %d\r\n\n", p2->name, p2->hitpoints);
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
            
                sprintf(buff, "Waiting for %s to strike...\r\n", p2->name);
                len2 = write(p->fd, buff, strlen(buff));
                if (len2 <= 0) {
                    // socket is closed
                    printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                    if(p->in_match){
                        sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
                        write(p2->fd, msg7, strlen(msg7));
                        write(p2->fd, msg8, strlen(msg8));
           
                        p2->in_match = 0;
                        p2->speak = 0;
                        p2->turn = 0;
                        p2->last_played = NULL;

                        p->in_match = 0;
                        p->speak = 0;
                        p->turn = 0;
                        p->last_played = NULL;
                    }

                    sprintf(outbuf, "**%s leaves**\r\n", p->name);
                    broadcast(top, outbuf, strlen(outbuf));
                    return -1;
                } 
                if(p2->pm == 0){
                    len3 = write(p2->fd, msg2, strlen(msg2));
                    if (len3 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                        if(p2->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                            write(p->fd, msg7, strlen(msg7));
                            write(p->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -2;
                    } 

                }else{
                    len3 = write(p2->fd, msg1, strlen(msg1));
                    if (len3 <= 0) {
                        // socket is closed
                        printf("Disconnect from %s\n", inet_ntoa(p2->ipaddr));
                        if(p2->in_match){
                            sprintf(msg7, "--%s dropped. You win!\r\n\n",p2->name);
                            write(p->fd, msg7, strlen(msg7));
                            write(p->fd, msg8, strlen(msg8));
           
                            p2->in_match = 0;
                            p2->speak = 0;
                            p2->turn = 0;
                            p2->last_played = NULL;

                            p->in_match = 0;
                            p->speak = 0;
                            p->turn = 0;
                            p->last_played = NULL;
                        }

                        sprintf(outbuf, "**%s leaves**\r\n", p2->name);
                        broadcast(top, outbuf, strlen(outbuf));
                        return -2;
                    } 
                }   
            }
            *start_time = time(NULL);
            return 0;
        } else if (!(p->in_match)|| !(p->turn)){
            return 0;
        }
    } else if (len <= 0) {
        // socket is closed
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        if(p->in_match){
            sprintf(msg7, "--%s dropped. You win!\r\n\n",p->name);
            write(p2->fd, msg7, strlen(msg7));
            write(p2->fd, msg8, strlen(msg8));
           
            p2->in_match = 0;
            p2->speak = 0;
            p2->turn = 0;
            p2->last_played = NULL;

            p->in_match = 0;
            p->speak = 0;
            p->turn = 0;
            p->last_played = NULL;
        }
        sprintf(outbuf, "**%s leaves**\r\n", p->name);
        broadcast(top, outbuf, strlen(outbuf));
        return -1;
    } 
    return 0;
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->in_match = 0;
    p->speak = 0;
    p->turn = 0;
    p->name = NULL;
    struct client *curr = top;
    if(curr == NULL){
        p->next = top;
        top = p;    
    }else{
        while(curr->next != NULL){
            curr = curr->next;
        }
        curr->next = p;
        p->next = NULL;
    }

    char name_msg[] = "What is your name? ";
    write(p->fd, name_msg, strlen(name_msg));
    
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    p = &top;
    if ((*p)->fd == fd){
        struct client *t = *p;
        (*p) = (*p)->next;
        free(t->name);
        free(t);

    } else {
        for (p = &top; *p && (*p)->next->fd != fd; p = &(*p)->next)
        ;
        // Now, p points to (1) top, or (2) a pointer to another client
        // This avoids a special case for removing the head of the list
        if (*p) {
            printf("It has reached %s\n", (*p)->next->name);
            struct client *t = (*p)->next;
            printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
            (*p)->next = t->next;
            free(t->name);
            free(t);
        } else {
            fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
        }

    }

    return top;
}


static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    char outbuf[512];
    int len2;
    for (p = top; p; p = p->next) {
        len2 = write(p->fd, s, size);
        if (len2 <= 0) {
            // socket is closed
            printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
            sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
            removeclient(top,p->fd);
            broadcast(top, outbuf, strlen(outbuf));
        } 
    }
    /* should probably check write() return value and perhaps remove client */
}

static void broadcast2(struct client *top, char *s, int size, struct client *p1) {
    struct client *p;
    char outbuf[512];
    int len2;
    for (p = top; p; p = p->next) {
        if(p1 != p)
        len2 = write(p->fd, s, size);
        if (len2 <= 0) {
            // socket is closed
            printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
            sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
            removeclient(top,p->fd);
            broadcast(top, outbuf, strlen(outbuf));
        }
    }
    /* should probably check write() return value and perhaps remove client */
}