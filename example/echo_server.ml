let handle_client c =
  let rec loop c =
    let header = Chan.read_char c in
    let count = Char.code header in
    let body = Chan.read_bytes c (count - 1) in
    Chan.write_char c header;
    Chan.write_bytes c body;
    Chan.flush c;
    loop c
  in
  try loop c
  with Unix.Unix_error(Unix.ECONNRESET, _, _)
     | End_of_file -> ()

let create_socket () =
  let open Unix in
  let sock = socket PF_INET SOCK_STREAM 0 in
  setsockopt sock SO_REUSEADDR true;
  setsockopt sock TCP_NODELAY true;
  bind sock (ADDR_INET (inet_addr_loopback, 8080));
  listen sock 10;
  set_nonblock sock;
  sock

let rec server sock =
  let clint_sock, _ = Unix.accept sock in
  Fiber.create handle_client (Chan.make clint_sock) |> ignore;
  server sock

let () =
  Fiber.create server (create_socket ()) |> ignore;
  Fiber.run ()
