## V-0.00
```C // @class: dhttp::http<br><br>```

## V-0.90

```C
// @action: parse http request from buffer
auto request = dhttp::http::parse()
// other methods
auto headers = request.headers()
auto body    = request.body()
auto buffer  = request.setbuf() // and getbuf
```
## V-1.0
```C
// Handle some header content
auto encode  = request.headers().has("encoding") 
bool valid   = request.body.validate_utf8(encode)
// other methods
bool test = request.headers.has_duplicate()
auto iter = request.iterator()
request.dump()
```
<br>
## V-1.01
```C
// Handle chunk data
bool succeed  = request.body.parse_chunk() 
// Streaming
auto request = dhttp::http::stream_parse()
// Building Request
auto buffer = dhttp::build_request::buffer()
buffer.add_header() // .remove_header
buffer.add_body()   // .remove_body
auto buffer = dhttp::build_request::raw()
buffer.build()
buffer.dump()
```