#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"


#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr;

    if (! mem->memory) {
        ptr = malloc(realsize + 1);
    } else {
        ptr = realloc(mem->memory, mem->size + realsize + 1);
    }
    if (! ptr) {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void wled_api_init(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    //curl_global_cleanup();
}

bool wled_xml_check(struct MemoryStruct *chunk)
{
    char *s = chunk->memory;
    /* XML parsing for dummies */

    if (strncmp(s, "<?xml", 5))
        return false;

    s = strstr(s, "<ds>");
    if (! s)
        return false;
    s += 4;

    if (strncmp(s, wled_ds, strlen(wled_ds)) != 0)
        return false;
    s += strlen(wled_ds);

    if (strncmp(s, "</ds>", 5) != 0)
        return false;

    return true;
}

bool wled_api_check(const char *addr)
{
    char url[7 + MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 4 + 1];
    CURL *curl_handle;
    struct MemoryStruct chunk = { NULL, 0 };
    CURLcode res;
    bool xmlok = false;

    snprintf(url, sizeof(url), "http://%s/win", addr);
    //fprintf(stderr, "curl: %s\n", url);

    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10);

    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        //fprintf(stderr, "%zu bytes retrieved\n", chunk.size);
        //fprintf(stderr, "%.*s\n", (int)chunk.size, chunk.memory);
        xmlok = wled_xml_check(&chunk);
        printf("wledapi: address: %s, %s-WLED: %s\n", addr, wled_ds, (xmlok ? "yes" : "no"));
    }

    curl_easy_cleanup(curl_handle);

    if (chunk.memory) {
        free(chunk.memory);
    }

    return xmlok;
}
