#include <stdio.h>
#include <stdlib.h> 
#include <curl/curl.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h> 




struct Server{

    char *country;
    char *city;
    char *provider;
    char *host;
    int id;

};
struct ServerWithSpeeds{
    struct Server *server;
    double download_speed;
    double upload_speed;
};

struct Servers{
    struct Server *servers;
    size_t size;
};
struct Memory {
  char *response;
  size_t size;
};


struct Servers read_file(){
    struct Servers result;
    result.servers = NULL;
    result.size = 0;
    FILE *file = fopen("speedtest_server_list.json", "r");
    if (file == NULL) {
        printf("Error: Unable to open the file.\n");
        return result;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *buffer = malloc(file_size + 1);
    if (fread(buffer, 1, file_size, file) != file_size) {
        printf("Error reading file.\n");
        free(buffer);
        fclose(file);
        return result;
    }
    buffer[file_size] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);


    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {

            printf("Error a: %s\n", error_ptr);
        }
        cJSON_Delete(json);
        return result;
    }
    size_t count = cJSON_GetArraySize(json);
    result.servers = malloc(sizeof(struct Server) * count);
    result.size = count;

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        cJSON *id = cJSON_GetObjectItem(item, "id");
        cJSON *host = cJSON_GetObjectItem(item, "host");
        cJSON *country = cJSON_GetObjectItem(item, "country");
        cJSON *city = cJSON_GetObjectItem(item, "city");
        cJSON *provider = cJSON_GetObjectItem(item, "provider");

        result.servers[i].id = id ? id->valueint : 0;
        result.servers[i].host = host && cJSON_IsString(host) ? strdup(host->valuestring) : NULL;
        result.servers[i].country = country && cJSON_IsString(country) ? strdup(country->valuestring) : NULL;
        result.servers[i].city = city && cJSON_IsString(city) ? strdup(city->valuestring) : NULL;
        result.servers[i].provider = provider && cJSON_IsString(provider) ? strdup(provider->valuestring) : NULL;
    }

    cJSON_Delete(json);
    return result;
}

static size_t write_cb(char *data, size_t size, size_t nmemb, void *clientp)
{
  size_t realsize = nmemb;
  struct Memory *mem = (struct Memory *)clientp;
 
  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if(!ptr)
    return 0;  
 
  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = '\0';
 
  return realsize;
}

static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  FILE *src = userdata;
  /* copy as much data as possible into the 'ptr' buffer, but no more than
     'size' * 'nmemb' bytes */
  size_t retcode = fread(ptr, size, nmemb, src);
 
  return retcode;
}

double convert_speed(double speed_in_bytes) {
    return (speed_in_bytes * 8) / (1024.0 * 1024.0); 
}
void server_info_print(struct Server server){
    printf("\nServer Info:\n");
    printf("Locatioin: %s, %s\n", server.country, server.city);
    printf("Provider: %s\n", server.provider);
    printf("Host: %s\n", server.host);
    printf("ID: %d\n", server.id);
}
double get_singular_download_speed( CURL *curl, struct Server server){
        
   
    struct Memory chunk = {0};
    char url[512];
    snprintf(url, sizeof(url), "http://%s/speedtest/random4000x4000.jpg", server.host);
    if (!curl) return 0.0;
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);  
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); 


    CURLcode res = curl_easy_perform(curl); 

    if(res == CURLE_OK) {
        double download_speed;
        curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &download_speed);

        printf("Download speed - %.2f Mb/s\n", convert_speed(download_speed));
        
        
        fflush(stdout);
        return download_speed;
    } else{
        printf("Download failed: %s\n", curl_easy_strerror(res));
    }
    free(chunk.response);

    return 0;
    
}
double get_singular_upload_speed(CURL *curl, struct Server server){

    char url[512];
    snprintf(url, sizeof(url), "https://%s/upload", server.host);
  

    if (!curl) return 0.0;
    curl_easy_reset(curl);


    FILE *src = fopen("upload_test_file.bin", "rb");
    if (!src) {
        printf("Error: Unable to open upload file.\n");
        return 0.0;
    }

    long file_size = 20971520;
    char *file_buffer = malloc(file_size);

   
    fclose(src);


    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, file_buffer);
    
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_size);


    curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);


    CURLcode res = curl_easy_perform(curl); 
    free(file_buffer);
    if(res == CURLE_OK) {
        double upload_speed;
        curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &upload_speed);

        printf("Upload speed: %.2f Mb/s\n", convert_speed(upload_speed));

        fflush(stdout);

        return upload_speed;
    }else{
        printf("Upload failed: %s\n", curl_easy_strerror(res));
    }

    return 0;
}
void get_multiple_internet_speed( CURL *curl, struct Servers servers){
    for(size_t i = 0; i < servers.size; i++){
        server_info_print(servers.servers[i]);

        get_singular_download_speed(curl, servers.servers[i]);

        double a = get_singular_upload_speed(curl, servers.servers[i]);

        

    }
}


struct ServerWithSpeeds find_best_server_by_location(char* country, char* city, struct Servers servers, CURL *curl){


    struct Servers categorised_servers;
    categorised_servers.servers = NULL;
    categorised_servers.size = 0;

    struct ServerWithSpeeds best_perf_server= {NULL, 0.0, 0.0};

     if(country == NULL && city == NULL){
        return best_perf_server;
    }

    
    for(size_t i = 0; i < servers.size; i++){
        bool both_not_null = (servers.servers[i].country != NULL &&
                              servers.servers[i].city != NULL &&
                              country != NULL &&
                              city != NULL);
        bool only_country_not_null = (country != NULL &&
                                      city == NULL);
        bool only_city_not_null = (country == NULL &&
                                   city != NULL);
        bool country_match = (only_country_not_null && strcmp(servers.servers[i].country, country) == 0);
        bool city_match = (only_city_not_null && strcmp(servers.servers[i].city, city) == 0);
        bool both_match = (both_not_null &&
                           strcmp(servers.servers[i].country, country) == 0 &&
                           strcmp(servers.servers[i].city, city) == 0);
        bool final_match = country_match || city_match || both_match;
        
        if(
            final_match
        ){
            server_info_print(servers.servers[i]);
            double download_speed = get_singular_download_speed(curl, servers.servers[i]);
            double upload_speed = get_singular_upload_speed(curl, servers.servers[i]);
            if(download_speed + upload_speed > best_perf_server.download_speed + best_perf_server.upload_speed){
                best_perf_server.server = &servers.servers[i];
                best_perf_server.download_speed = download_speed;
                best_perf_server.upload_speed = upload_speed;
            }
          
        }
    }
    if(best_perf_server.server == NULL){
        printf("No servers found for the specified location.\n");
        return best_perf_server;
    }
    printf("\nBest server by download/upload speed in %s, %s:", 
           country ? country : "any country", 
           city ? city : "any city");
    server_info_print(*best_perf_server.server); 
    printf("Download speed: %.2f Mb/s\n", convert_speed(best_perf_server.download_speed));
    printf("Upload speed: %.2f Mb/s\n", convert_speed(best_perf_server.upload_speed));
        
    return best_perf_server;

}

struct Server get_my_location(CURL *curl){
    struct Server my_location = {NULL, NULL, NULL, NULL, 0};
    
    if (!curl) return my_location;

    struct Memory chunk = {0};
    curl_easy_reset(curl);

    curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json/");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    CURLcode res = curl_easy_perform(curl);
    if(res == CURLE_OK) {
        cJSON *json = cJSON_Parse(chunk.response);
        if (json != NULL) {
            cJSON *country = cJSON_GetObjectItem(json, "country");
            cJSON *city = cJSON_GetObjectItem(json, "regionName");
            cJSON *isp = cJSON_GetObjectItem(json, "isp");
            if (country && cJSON_IsString(country)) {
                my_location.country = strdup(country->valuestring);
            }
            if (city && cJSON_IsString(city)) {
                my_location.city = strdup(city->valuestring);
            }
            if (isp && cJSON_IsString(isp)) {
                my_location.provider = strdup(isp->valuestring);
            }
            cJSON_Delete(json);
            
        }
    }
    free(chunk.response);
    return my_location;
}

void find_server_by_id(int id, struct Servers servers, CURL *curl){
    for(size_t i = 0; i < servers.size; i++){
        if(servers.servers[i].id == id){
            server_info_print(servers.servers[i]);
            get_singular_download_speed(curl, servers.servers[i]);
            get_singular_upload_speed(curl, servers.servers[i]);
            return;
        }
    }
    printf("Server with ID %d not found.\n", id);
}

void find_server_by_url(char* url, struct Servers servers, CURL *curl){
    for(size_t i = 0; i < servers.size; i++){
        if(strcmp(servers.servers[i].host, url) == 0){
            server_info_print(servers.servers[i]);
            get_singular_download_speed(curl, servers.servers[i]);
            get_singular_upload_speed(curl, servers.servers[i]);
            return;
        }
    }
    printf("Server with URL %s not found.\n", url);
}


int main(int argc, char *argv[]) {

    CURL *curl = curl_easy_init();

    struct Servers servers = read_file();
    struct Server my_location = get_my_location(curl);

    int opt;
    int any_option_given = 0;

    char *city = NULL;
    char *country = NULL;

    while((opt = getopt(argc, argv, ":ao:i:s:u:")) != -1) 
    { 
        switch(opt) 
        { 
            case 'a':
                any_option_given = 1;
                printf("Checking all servers:\n");
                get_multiple_internet_speed(curl, servers);      
                break; 
            case 'o': 
                any_option_given = 1;
                country = optarg;
                break; 
            case 'i': 
                any_option_given = 1;
                city = optarg;
                break;
            case 's':
                any_option_given = 1;
                printf("Finding best server by id: %d\n", atoi(optarg));
                find_server_by_id(atoi(optarg), servers, curl);
                break;
            case 'u':
                any_option_given = 1;
                printf("Finding best server by url: %s\n", optarg);
                find_server_by_url(optarg, servers, curl);
                break;
            case ':': 
                printf("option needs a value\n"); 
                break; 
            case '?': 
                printf("unknown option: %c\n", optopt);
                break; 
            default: 
                break;
        } 
    }
    if(country != NULL || city != NULL){
        find_best_server_by_location(country, city, servers, curl);
    }
    if(!any_option_given){
        printf("Finding best srver for your location: Country = %s, City = %s\n", my_location.country, my_location.city);
        find_best_server_by_location(my_location.country, my_location.city, servers, curl);
        
    }

   
    return 0;
}
