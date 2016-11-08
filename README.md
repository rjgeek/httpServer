bham-web-server
==========
RuJia

A multithreaded HTTP web server that provides access to web services and files. 
Compliant with HTTP/1.1. The services provided follow the REST architecture style.
The main() method starts by parsing out the command line arguments.It then sets up 
the TCP protocol by creating a socket, binding it and listening to it. 
An accept() call is called in an infinite-loop which awaits incoming requests. 
An incoming request is duplicated into a different socket to keep the main socket available for more 
connections, and the HTTP transaction is dispatched into a new thread-pool
Support static and dynamic languages, but dynamic languages parsing have not finished


# Usage

## Make,Run,Test

    $./script.sh
    
## Functionality

### Provide arguments when start
-h prints help   
-p port to accept HTTP client requests (required)  
-f path that specifies the root directory for serving files  

### Multiple Threaded Support
To support multiple clients, each request is dispatched in a new thread while the main thread is awaiting new 
requests. Our thread pool is utilised here.

### http1.1 Protocol Support

#### Content-length
you can see it from response header
#### Content-type
type:text/html,text/javascript,text/json,image/gif,image/png,image/jpeg,text/css,text/plain
#### Date
you can get server current time
#### Expires
you can get file Expires time
#### Pragma
Implementation-specific fields that may have various effects anywhere along the request-response chain.
#### Server
A name for the server
#### Version 
version http1.1

### get
provide get method , you can visit as this: http://127.0.0.1:8088/files/index.html?t=11
### post
provide post method, cur -d "v=1" http://127.0.0.1:8088/files/index.jsp

### Serving Files
To support serving files, our server takes an -R argument which determines the path to the root directory 
of our server. The default path is /files. The server prevents requests to /files/../../ (up one directory)
to avoid serving other files on the server. If the path given is a valid path, the file is served to the 
client as a response.

## Special Note
As our teacher said #java is :shit:#, This is my first time to write code by C,
So I have to admit I quote some other people´s code
which thread pool is provide by nayefc and memmory is provide by pintos

##Reference
https://web.stanford.edu/class/cs140/projects/pintos/pintos_1.html  
https://github.com/nayefc/web-server  
http://blog.chinaunix.net/uid-27164517-id-3364985.html  




















