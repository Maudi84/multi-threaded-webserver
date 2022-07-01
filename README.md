# Readme
## ./httpserver.c <port number>

This program is designed to take 3 http requests and respond to them.
- GET writes out information from a selected file.
- PUT overwrites infromation to a selected file.
- APPEND appends information to a selected file.

## DESIGN
This program is a continious loop within a loop. The outer loop waiting for a socket response and the inner loop processing the client requests. A client's entire request is processed in a single loop, that way any subsequent requests can be handled within on the next loop around and only broken when the client breaks socket connection.

Once the request is received, it is parsed and determined whether or not to be a legitimate request. If the request can be carried out, the request is completed the client receive's a OK response, or the data they requested. If the request is invalid, the client receive's the appropriate error response.

The program is split between two main modules "io" and "util", util handles the parsing and verification of the request and header fields. io moves information back and forth between file and socket and vice versa. httpserver serves as an interface between these two modules. There are various keys which are passed between the modules in order to signify how the other module should behave.

For example:, util may send informtion to httpserver that a client's request is for a file that is a directory, httpserver will then send that information to io so that it can send the appropriate error response (403, forbidden) for that error.

This seemed appealing on a design level because both the io operations and string parsing operations are the most resource consuming for my program and putting them into seperate modules showed promise for parallelism in the future.
## Rundown
When a socket connection is found, the loop begins and the information is read into a buffer, the amount of bytes read in is kept track of. The request is then verified by passing this buffer into the util module. The util module verifies the request by first parsing the method, uri, and version into seperate strings, the amount of bytes allowed into each one of these string is limited. Each string is tested individually by checking the size and format using regex. 

If any of the three strings fail any of the tests, then a response is sent to httpserver which tells httpserver to invoke an error from io, io invokes the correct error response depending on the code. At anytime an error message is sent, the inner loop resets and waits to see if the client sents another request. Because the resources to parse these strings is significant, anytime a piece of the request is not correct an error is immediately sent and the request is "thrown away". If a request is not properly formatted then a bad request error is sent. If there is an interal error such as a regex failure, then a 500 internal error response is sent.

If the format of the request line is correct then the method is validated. If the method is not impelemented, then there is no point in wasting resources, if the method is not found then the "not implemeneted" error is sent and the request is thrown away. 

If the method is valid then the path is checked, the file can throw various errors including forbidden 403 if its a directory or eacces is thrown or 404 if the file doesn't exist. Even more resources are required to parse the header field, so its imperative to make sure a request can be carried out before continuing on. 

If the request is a PUT or APPEND, or there are bytes after the intial header, we check for a content-length and throw a bad format error if it doesn't exist or if it is mal formatted. There is also a bad request error if there are more bytes than whats stat in content-length after the iintiial request header. 

At this point we have checked the format of everything in the intiial header request and verified it to be properly formatted, using util to parse and io to send errors if needed.

Once we're 100% sure that our request can actually be carried out, we carry it out. For a GET request, the bytes of a file are used for a content_length and also a guide for how many bytes to send over a socket. Because sockets have unexpected behavior, keeping track of the bytes is important. for PUT and APPEND, the conternt_length of the request was grabbed and used to keep track of how many bytes to read from the socket. There are also fail safes to help functions "break out" in the even of a socket read and no information available. 

These io functions ocassionally invoke 500 interal error codes when they run into unexpected issues. inside the io module. This is because the server must continue running, so error codes that would normally invoke an error response and program end are instead responded to with 500 errors. 

They have loops that rely on bytes getting read in and out correctly so that nothing is mismatched. 

## Error Logging Added

Error logging was added with the intention of keeping a record of all of the request transactions made on the server. A Request-Id: header field was created in order to correspond log entries with request identities. Some of the existing modules in the program served useful to creating the logging. New modules, such as a module that finds and extracts the request id was added into my existing bigger modules like my header verification. With most of the leg work being done by existing and new modules, the write and flush calls to the log were being performed in the httpserver interface right before the data transfer is carried out.

This way if there is a termination of the socket or from the server or an error, the valid request still shows up in the log. 

Because there were already strings created for my method and uri, once I created an extraction for the request id, the only thing left was the status codes which are determined at the point of execution inside the httpserver interface.

A module in util concats the string together into a complete error log message. Once the log is created, it is immediately written using fwrite and flushed into the logfile stream using fflush. Then the desired request is performed. 

The module for extraind the request id was placed into the existing header module. 

## Multi Threading Added

Unfortunately I have spent the last 2-3 days trying to fix my pipeline which turned out to be an error in the pipeline and not my code. I have not been able to focus much on my implementation due to this.

For the time being I am supporting a traditional producer consumer queue setup. The dispatcher thread places jobs inside of a que which workers take turn grabbing. Unfortunately I have not had a chance to get my handle connection function ready for multiple threads at this point.

 I plan on switching it to handle multiple threads and also to get rid of any blocking behavior by the next road map.
 
## Atomicity and Coherency

Many big changes were made to my program since the last debacle with the pipeline. I found that many data races exsisted inside of my util.c file. Many of the features which I used to process my requests were sharing global variables. 

Many lines of code were deleted and rewritten to get all of the memory keeping inside of the main module which would stop the data races. I also changed my handle connections function from a series of complicated if else statements to an enumerated selection process. This made the code much cleaner to read and also set me up to implement scheduling in the futrure. Looking back, if I had known more about multi-threaded programming I could have made some of these changes early on. 

The fixing of my data races seemed to fix a lot of my test results, I also found another error where a thread would sometimes block my dispatcher thread and prevent it from queueing jobs. 

Unfortunately, test 1 proved to consistently pass about 60% of the time. I could never nail down exactly what was causing my issue. I ran a series of tests that would mimic the described testing, the tests seemed to run correctly, however I continued to fail on the pipeline. These tersts included bash scripts and oliver twist toml that would run 1000 random concurrent requests and monitor the output. I would also monitor my thread activity. These both seemed normal. There were also other students who mentioned this test was difficult. It is tough to say if a partially inactive thread or some type of misresponse from my server during 1000 requests was causing the problem, but the issue was never identified. 

The second to last test also proved to be inconsistent until I implemented a strategy for file coherency and response / log atomicity. This solution appeared to finally solve this test and I began to consistently pass.

Finally, the last test. I tried to find a solution to get this test to pass without implementing non-blocking io / scheduling algorithm. I came to the conclusion that I would most likely need a non-blocking / scheduling algorithm in order to solve this problem. This would prevent these threads from blocking during a multiple read/write to the same uri. Unfortunately at this stage in the program I no longer had time to fully impelment and test a complete non-blocking io system with rescheduling. 

The hidden tests proved to be difficult to past. Mostly to due with their hidden nature, but also to do with the fact that the testing and program becomes so complex that it is sometimes difficult to make changes without seeing a dramatic change in other tests. Also, with the inconsistency of some of the other tests, it was difficult to know which changes were making a positive overall effect for the program. It was also mentioned by Dr. Q that some of these tests could potentially require non-blocking io, which possibly could explain not being able to pass some of them as well.

###Atomicity and Coherency Additions

For atomicity between logs and responses, the issue is that the real time a client can receive a response vs the virtual time that its sent can be drastically different. In order to give the feel of an atomic operation, I decided that both log file entry and client response should be placed into a mutex together. This ensures that only one thread may send a response / log combination at a time and essentially makes this step of the operation sequential. This does create a bottle neck towards the end of the method, however, the client response and log file entry are both very small operations and is a trade off that must be made in order to make these operations atomic.

The second and more difficult issue was file reading and writing coherency. Since threads operate side by side, they sometimes contend to do operations to the same file. In order to have coherent read and writes it was necessary to place reader writer locks on the sections of the code where the threads were accessing files. Since both appends and puts did writes, they were given exclusive flocks on the section of the code where they wrote to files. the GET requests were given shared locks that allowed other readers (GET requests) to do operations without holding an exclusive lock.

This flock essentially serves as a mutex that is specific to a uri, and doesn't hold up the entire pipeline with a general mutex. 

The ultimate design was to place a flock that went directly into a mutex. Anytime a put, or append tried to write to a file, they would get an exclusive lock which would enter them directly into a mutex to write their response and log. This made it so that reads and writes to the same file and their respective log response were always atomic. If any method grabbed the flock for a particular uri first, they were also ensured to also write the response / log first as well. After this implementation was put into place, I noticed the second to last test was now consistently passing.

The final tradeoff that was made was the decision not to put GET socket responses and log writes into a mutex together. This would cause the GET method to delay during a big socket write, which I felt was not necessary to stop the entire pipeline for.

###Future additions and unpassed tests

The last test I was not able to pass and I suspect the reasoning behind it was because I was using blocking IO operations throughout my program. If I had more time to work on this issue I would have developed a scheduling system that allowed threads to go in and out of function and only do work when io methods were not blocked.

I would also have liked to implement a system where threads used temporary files as buffers, this would allow threads to spend less time in critical sections and quickly pass the information in the temporary file to its destination. It would also help in a scheduling system when threads have to exit the functions and return back at a later time to read information from a socket.

The last addition I would have liked to make is an epoll system. This would keep my dispatcher thread from being blocked and help prevent denial of service attacks on a system by allowing a malicious user to keep my dispatcher thread blocked on an attack of connections.




 


