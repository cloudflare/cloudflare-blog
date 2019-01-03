Two simple programs using Linux AIO interface:

aio_passwd.c reads /etc/passwd. It doesn't do it in truly async way
(since the file is not opened with O\_DIRECT), but it's good to
illustrate the blocking nature of io_submit.

aio_poll.c uses IOCB\_CMD\_POLL to wait for a packet from a networking
socet, in truly async way. The io\_submit does not block and we are
blocking in io\_getevents, awaiting POLLIN event notification. This
requires kernel 4.18 or newer.

