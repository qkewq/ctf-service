# ctf-service
The ctf-service application allows you to host multiple custom applications easily and persistently for capture the flag challanges.

## Services

When a competitor connects to the server on one of the specified service ports and addresses, the `ctf-service` will execute the service located at the specified `filepath`.  The client file descriptor will always be passed as `argv[1]` to the service in character form, i.e `atoi(argv[1])` is the file descriptor the service can use to communicate with the client.  Upon completion the service should exit like normal.  Processes are spawned as clients connect.

## Config File

The default location for the config file is `/etc/ctf-service.conf` and can be changed by passing the config file path as the first argument of the program.

The file is made of of `[[Service]]` sections, each containing configurations for that specific service.  Keys are:

|Key|Note|
|---|---|
|`name`|Name of the program passed as `argv[0]`|
|`filepath`|/path/to/program to execute|
|`type`|Only `executable supported for now|
|`port`|Port number for the service to listen on|
|`addr`|IPv4 address to listen on|
|`addr6`|IPv6 address to listen on|

At least one `addr` key must be present and multiple of each are allowed.  Future `types` my include execution for python programs or UDP based services.

### Syntax

- Whitepsace is ignored between keys and values
- Keys and values are separated by an equals sign `=`
- Comments are lines with `#` or `;` as the first non-whitespace character
- Key value pairs must be under a `[[Service]]` header
- There can be multiple of both `addr` and `addr6` keys per service

### Example Config

```ini
# Example Config File

[[Service]]
name=example_service
filepath=/opt/ctf/example.out
type=executable
port=1337
addr=0.0.0.0
addr6=::

[[Service]]
name = another_example
filetype = /opt/ctf/another.out
type = executable
port = 50000
addr = 192.168.1.20
addr = 127.0.0.1
addr6 = ::1
```

## Future Additions

- Rate limiting
- Other `types` like python
- Starting UDP services
- Handling services that want to use two file descriptors, ex. ftp
