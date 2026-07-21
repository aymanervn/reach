#ifndef RTV_RETRIEVER_H
#define RTV_RETRIEVER_H

/* retriever -- instant filename search service
 *
 * This header documents the complete client protocol of retriever.exe and
 * defines the constants needed to implement a client. The service compiles
 * against this same file, so it cannot drift from the running binary.
 *
 *
 * 1. TRANSPORT
 *
 * The service listens on a local byte-stream endpoint:
 *
 *   Windows:  named pipe RTV_PIPE_NAME, duplex byte mode. Open it with
 *             CreateFile (GENERIC_READ | GENERIC_WRITE); if the open fails
 *             with ERROR_PIPE_BUSY, WaitNamedPipe and retry. The pipe ACL
 *             grants read/write to authenticated users, so ordinary
 *             processes can talk to the elevated service.
 *   POSIX:    unix domain socket RTV_SOCK_PATH.
 *
 * A connection carries any number of requests, answered in order. Each
 * connection is served by its own thread; searches from concurrent
 * connections run in parallel.
 *
 *
 * 2. FRAMING
 *
 * Requests and responses are UTF-8 text lines terminated by '\n' ("\r\n"
 * is accepted on requests). A request is one line; the service reads at
 * most 4095 bytes of it and silently drops the rest. No line in either
 * direction exceeds RTV_MAX_LINE bytes, so it is a safe buffer size.
 * An empty request line is ignored and produces no response.
 *
 * Every response is a sequence of lines ending with a line containing a
 * single '.'. The first byte of each line before the terminator says what
 * it is:
 *
 *   '*'  status line, one per response (e.g. "* 12 hits in 480 us")
 *   '!'  error line; the response carries no results
 *   else a result line (an absolute path)
 *
 *
 * 3. REQUESTS
 *
 *   PING
 *     -> "* pong <version>"
 *
 *   STATS
 *     -> "* files=N dirs=N vols=N records=N mem_bytes=N"
 *
 *   RESCAN
 *     Drops and re-enumerates every indexed volume. Blocks until done
 *     (can take seconds).
 *     -> "* rescanned N volumes in T us"
 *
 *   S <mode> <max> <query>
 *     Search. <max> is the result cap as a decimal number, 0 means the
 *     server default (100000). <query> is everything up to the end of the
 *     line, spaces included. Matching is ASCII case-insensitive.
 *     -> "* N hits in T us" followed by one path per result line.
 *
 *     Modes:
 *       name    substring of the file name
 *       prefix  file name starts with the query
 *       suffix  file name ends with the query (handy for extensions)
 *       exact   file name equals the query
 *       path    substring of the full path (slower: builds each path)
 *       regex   the query is a shape pattern matched against the whole
 *               file name:
 *                 \d  a number (one or more digits)
 *                 \a  one or more letters
 *                 \p  one or more special characters (not letter/digit)
 *                 \\  a literal backslash
 *               any other character matches itself.
 *               Example: "img_\d.jpg" matches "IMG_20260720.jpg".
 *
 *     Errors: "! bad request", "! unknown mode '<m>' (have: ...)",
 *     "! bad pattern: <reason>".
 *
 *   Anything else -> "! unknown command".
 *
 *
 * 4. EXAMPLE SESSION
 *
 *   -> S suffix 3 .pdf
 *   <- * 3 hits in 2231 us
 *   <- C:\Users\a\Documents\report.pdf
 *   <- C:\Users\a\Downloads\invoice.pdf
 *   <- C:\books\c11.pdf
 *   <- .
 */

#define RTV_VERSION "0.9.5"
#define RTV_PIPE_NAME "\\\\.\\pipe\\retriever"
#define RTV_SOCK_PATH "/tmp/retriever.sock"
#define RTV_MAX_LINE 4128

#endif
