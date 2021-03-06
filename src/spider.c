#include "spider.h"


pthread_mutex_t mutex_spider = PTHREAD_MUTEX_INITIALIZER;

void spider(void *in)
{
	pthread_mutex_lock(&mutex_spider);
	char *line=(char *)in;

	struct MemoryStruct chunk;
	long status = 0,length = 0;
	int res = 0, try = 1, POST = 0, timeout = 3, debug_host = 3; 
	char *make = NULL, *make2 = NULL, *make_cookie = NULL, *make_agent = NULL;
	char *token = NULL, *request_file = NULL;


	if (param.timeout!=NULL)
		timeout = (int)strtol(param.timeout,(char **)NULL,10);

// if need get anti-csrf token
	if(param.host!=NULL && param.token_name!=NULL)
	{
		token = xmalloc(sizeof(char)*2048); 
		memset(token,0,2047); // note: last byte for canary, hardening makefile...
		token = get_anti_csrf_token(param.host,param.token_name);
	}



// payload tamper, get payload of line and make tamper 
	if(param.tamper!=NULL)
		line = tamper_choice(param.tamper,line);

	chomp(line);

// goto to fix signal stop if user do ctrl+c
	try_again:

	POST=param.post?1:0;

	while(try)
	{
		CURL *curl;  
//		curl_global_init(CURL_GLOBAL_ALL); 

		chunk.memory = NULL; 
		chunk.size = 0;  

		curl_socket_t sockfd; /* socket */
		long sockextr;
		size_t iolen;


		curl = curl_easy_init();
		
// add payload at inputs
		if(param.custom==NULL) //if custom request  argv mode null
		{
			make2 = replace ( (POST?param.post:param.host),"^",line);

			if (token)
		 		make = insert_csrf_token(make2, param.token_name, token); 
			else
				make = xstrndup(make2, strlen(make2));	

			if (param.cookie!=NULL)	
				make_cookie = replace( param.cookie,"^",line);	
	
			if (param.UserAgent!=NULL)
				make_agent = replace( param.UserAgent,"^",line);

			curl_easy_setopt(curl,  CURLOPT_URL, param.host);
		} else {
// if is custom request
			request_file = read_lines(param.custom);
			make2 = replace( request_file,"^",line);	
			curl_easy_setopt(curl,  CURLOPT_URL, param.host);

			if (token!=NULL)
				make = insert_csrf_token(make2, param.token_name, token);

			else
				make = xstrndup(make2, strlen(make2));

			XFREE(request_file);
		}	
 
		if ( POST )
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, make);
      
		curl_easy_setopt(curl,  CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl,  CURLOPT_WRITEDATA, (void *)&chunk);

// load user agent     
		if ( param.agent!=NULL )
		{
			curl_easy_setopt(curl,  CURLOPT_USERAGENT, param.agent);
		} else {
			curl_easy_setopt(curl,  CURLOPT_USERAGENT, "Mozilla/5.0 (0d1n v0.1) ");
		}

// json headers to use JSON

		if (param.header!=NULL)
		{
			struct curl_slist *headers = NULL;
			curl_slist_append(headers, param.header);

			if (param.json_headers==true)
			{
				curl_slist_append(headers, "Accept: application/json");
				curl_slist_append(headers, "Content-Type: application/json");
			}

			curl_easy_setopt(curl,  CURLOPT_HTTPHEADER, headers);
			curl_slist_free_all(headers);

		} else {

			if (param.json_headers==true)
			{
				struct curl_slist *headers = NULL;

				curl_slist_append(headers, "Accept: application/json");
				curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl,  CURLOPT_HTTPHEADER, headers);
				curl_slist_free_all(headers);
			}
		}
	
//use custom method PUT,DELETE...
		if (param.method!=NULL)
			curl_easy_setopt(curl,  CURLOPT_CUSTOMREQUEST, param.method);
 
		curl_easy_setopt(curl,  CURLOPT_ENCODING,"gzip,deflate");

// load cookie jar
		if (param.cookie_jar != NULL)
		{
			curl_easy_setopt(curl,CURLOPT_COOKIEFILE,param.cookie_jar);
			curl_easy_setopt(curl,CURLOPT_COOKIEJAR,param.cookie_jar);
		} else {
			curl_easy_setopt(curl,CURLOPT_COOKIEJAR,"odin_cookiejar.txt");
		}
// LOAD cookie fuzz

		if (param.cookie!=NULL)
			curl_easy_setopt(curl,CURLOPT_COOKIE,make_cookie);


// LOAD UserAgent FUZZ
		if (param.UserAgent!=NULL)
			curl_easy_setopt(curl,CURLOPT_USERAGENT,make_agent);


		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1);
// Load cacert
		if (param.CA_certificate != NULL) 
		{
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
			curl_easy_setopt(curl, CURLOPT_CAINFO, param.CA_certificate);
		} else {

			curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L); 
			curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L); 
		}

		if (timeout) 
			curl_easy_setopt(curl,CURLOPT_TIMEOUT,timeout); 

// load single proxy
		if (param.proxy != NULL)
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, param.proxy);
	//		curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1);
		}

// load random proxy in list 
		if (param.proxy_rand != NULL)
		{
			char *randproxy = Random_linefile(param.proxy_rand);
			curl_easy_setopt(curl, CURLOPT_PROXY, randproxy);
	//		curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1);
		}

// choice SSL version
		if ( param.SSL_version != NULL ) 
			curl_easy_setopt(curl,CURLOPT_SSLVERSION,(int)strtol(param.SSL_version,(char **)NULL,10));

		curl_easy_setopt(curl,CURLOPT_VERBOSE,0); 
		curl_easy_setopt(curl,CURLOPT_HEADER,1);  
		
// if use custom request
		if (param.custom!=NULL)
			curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
		

		res = curl_easy_perform(curl);
// get HTTP status code
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,&status);

// custom http request
		if (param.custom!=NULL)
		{
			curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr); 
			sockfd = sockextr;

			if (!wait_on_socket(sockfd, 0, 60000L))
				DEBUG("error in socket at custom http request");
			
			res = curl_easy_send(curl, make, strlen(make), &iolen);
// recv data of custom request
			while (1)
			{
				wait_on_socket(sockfd, 1, 60000L);
				chunk.memory = xmallocarray(3024,sizeof(char)); 
				res = curl_easy_recv(curl, chunk.memory, 3023, &iolen); 
				chunk.size = strnlen(chunk.memory,3023);				

				if (strnlen(chunk.memory,3023) > 8)
					break;

			        if (CURLE_OK != res)
        				break;

			}

			
			status = (long)parse_http_status(chunk.memory);
//status=404;
		}

			

// length of response
		if (chunk.size<=0)
			length=0.0;
		else
			length=chunk.size;

// if have error at status
	
		if (status==0)
		{	
			debug_host--;
			DEBUG("Problem in Host: \n %s\n Host is down ? %s\n", chunk.memory, strerror(errno));
 			DEBUG("curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
			if (debug_host==0)
				exit(0);

			sleep(3);

			goto try_again;
			
		
		}


		debug_host=3;
		try--;


		curl_easy_cleanup(curl);
	}
	// Write results in log and htmnl+js in /opt/0d1n/view
	write_result(	(char *)chunk.memory,
			param.datatable,
			line,
			make,
			make_agent,
			make_cookie,
			status,
			length
	);	

	// clear all
	XFREE(chunk.memory);
	XFREE(make_agent);
	XFREE(make_cookie);
	XFREE(make);
	XFREE(make2);

	if(param.tamper != NULL)
		XFREE(line);

	if(param.token_name != NULL)
		XFREE(token);


	pthread_mutex_unlock(&mutex_spider);
}

