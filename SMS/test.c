//kenny ellis
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "util.h"
#include "multi-lookup.h"
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

queue names;
char* output_file;
FILE* output_file_path = NULL;
int push_to_q = 1; 

      //ptheads  ----------------------------
pthread_mutex_t q_access;                           //--> mutex
pthread_mutex_t write;          
pthread_cond_t wait_q_empty; //wait till q is empty   --> conditions
pthread_cond_t wait_q_full; //wait till q is full


//////////////////////////////////////////////////////////////////////////



void *resolver(){                                                   //resolver
      char *host_name = NULL;
      char IP_string[MAX_IP_LENGTH];
    
      while(!queue_is_empty(&host_name) || push_to_q){
          pthread_mutex_lock(&q_access);

            while(queue_is_empty(&host_name)){
                pthread_cond_wait(&wait_q_empty, &q_access);
            }

            host_name = queue_pop(&host_name);
            pthread_cond_signal(&wait_q_full);

          pthread_mutex_unlock(&q_access);
          pthread_mutex_lock(&write);                                                     // write to file + IP lookup 
         
            if(dnslookup(host_name, IP_string, sizeof(IP_string)) == UTIL_FAILURE){      // get IP_string + host_name find
                fprintf(error2, "ERROR! There was a DNS lookup error: %s\n", host_name);
                strncpy(IP_string, "", sizeof(IP_string));

            }

            // output_file write  -----------------------
            fprintf(output_file_path, "%s,%s\n", host_name, IP_string);
            free(host_name);
          pthread_mutex_unlock(&write);
      }

    pthread_exit(0);

};



////////////////////////////////////////////////////////////////////////////////////////////



void *requester(void* input){             // requester

    char error[MAX_NAME_LENGTH];          // MNL = 1025 Characters, including null terminator
    char host_name[MAX_NAME_LENGTH]; 
    char *name_of_file = (char*)input;
    FILE* input_file_path = NULL;
    input_file_path = fopen(name_of_file, "r");                        //open - read


    if(!input_file_path){
        sprintf(error, "Error Opening Input File: %s", name_of_file);  //(sprintf)Composes a string. the content is stored as a C string in the buffer pointed by str.
        perror(error);                                                 //(perror)print error message

    }

    while(fscanf(input_file_path, INPUTFS, host_name) > 0){            //fscanf Reads data from input_file_path and stores  according to parameter format into the locations given by the additional arguments, as if scanf was used, but reading from s.
        char * host= strdup(host_name);
        pthread_mutex_lock(&q_access);                                 //lock

           while(queue_is_full(&host_name)){
                pthread_cond_wait(&wait_q_full, &q_access);    //wait

            }

            //push to q  --------------------
            queue_push(&host_name, (void*) host );
            pthread_cond_signal(&wait_q_empty);               //signal
        pthread_mutex_unlock(&q_access);
    }

    fclose(input_file_path);

    pthread_exit(0);

};



////////////////////////////////////////////////////////////////////////////////




int main(int argc, char* argv[]){  //main

        // check to see if valid # of arguments --------------------------
    if(argc < minimum_arguments){             
      fprintf(error2, "ERROR! Invalid Number Of Arguments: %d Arguments Entered , %d Arguments Necessary ", (argc - 1), (minimum_arguments - 1));
      fprintf(error2, "ERROR! Invalid Use: \n %s %s", argv[0], USAGE);
      return EXIT_FAILURE; //fail

    };

    // output_file is now opennn -------------------------
    output_file_path = fopen(output_file, "w");   //open
    output_file = argv[argc - 1];
    

    if(!output_file_path){

      fprintf(error2, "ERROR! Error With File Opening!");
      return EXIT_FAILURE;
    }

       //mutex intialization, condition initialization, queue intialization ------------------
    
    
    pthread_mutex_init(&write, NULL);           //mutexs
    pthread_mutex_init(&q_access, NULL);

    pthread_cond_init(&wait_q_empty, NULL);     //conditions
    pthread_cond_init(&wait_q_full, NULL);

    queue_init(&host_name, max_q_size);         //queue

        // threads
    int pcount = sysconf(_SC_NPROCESSORS_ONLN);   //pcount = processor count //  _SC_  = The number of processors currently online (available).
    int resolver_thread_count = (pcount >= MIN_RESOLVER_THREADS ? pcount : MAX_RESOLVER_THREADS);   // must always provide at least 2 resolver threads = min 
                                                                                                    //10 Threads (This is an optional upper-limit.) = max

    pthread_t requester_thread[argc - 2]; //decrement
    pthread_t resolver_thread[resolver_thread_count];

        //thread_responses: 0 success

    int trcount;
    int thread_response; 
   

      
        //thread_response threads
    for (trcount = 0; trcount< resolver_thread_count; trcount++){
        thread_response = pthread_create(&resolver_thread[trcount], NULL, resolver, NULL);

        if(thread_response){
          fprintf(error2, "ERROR! There Was An Error With Thread creation: # %d, The Error's Code: %d", trcount, thread_response);
          exit(EXIT_FAILURE);

        };

    };

     //requester_thread threads
    for (trcount = 0; trcount< (argc-2); trcount++){
        thread_response = pthread_create(&requester_thread[trcount], NULL, requester, (void*) argv[trcount + 1]);

        if(thread_response){
          fprintf(error2, "ERROR! There Was An Error With Thread Creation: # %d, The Error's Code: %d", trcount, thread_response);
          exit(EXIT_FAILURE);

        };

    };


        // waiting
    for(trcount = 0; trcount < (argc - 2); trcount++){
      thread_response = pthread_join(requester_thread[trcount], NULL);

      if(thread_response){
        fprintf(error2, "ERROR: There Was An Error While Waiting For Thread: # %d, The Error's Code: %d ", trcount, thread_response);
        exit(EXIT_FAILURE);

      };

    };

    push_to_q = 0; //all items pushed onto the queue

        // resolver_threads threads + counter
    for(trcount = 0; trcount < resolver_thread_count; trcount++){
      thread_response = pthread_join(resolver_thread[trcount], NULL);

      if(thread_response){
        fprintf(error2, "ERROR: There Was An Error While Waiting For Thread: # %d, The Error's Code: %d", trcount, thread_response);
        exit(EXIT_FAILURE);

      };

    };


    fclose(output_file_path);
    queue_cleanup(&host_name);
    pthread_mutex_destroy(&q_access);
    pthread_mutex_destroy(&write);
    pthread_cond_destroy(&wait_q_empty);
    pthread_cond_destroy(&wait_q_full);
    exit(EXIT_SUCCESS);


    
};