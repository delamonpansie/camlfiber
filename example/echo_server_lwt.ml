open Lwt

let handle_message ic oc =
  let rec loop () =
    let%lwt header = Lwt_io.read_char ic in
    let count = Char.code header in
    let body = Bytes.create count in
    let%lwt () = Lwt_io.read_into_exactly ic body 0 (count - 1) in
    let%lwt () = Lwt_io.write_char oc header in
    let%lwt () = Lwt_io.write_from_string_exactly oc body 0 (count - 1) in
    loop ()
  in
  try%lwt loop ()
  with Unix.Unix_error(Unix.ECONNRESET, _, _)
     | End_of_file -> Lwt.return_unit

let create_socket () =
  let open Lwt_unix in
  let sock = socket PF_INET SOCK_STREAM 0 in
  let%lwt () = bind sock (ADDR_INET (Unix.inet_addr_loopback, 8080)) in
  listen sock 10;
  return sock

let rec server sock =
  let%lwt clint_sock, _ = Lwt_unix.accept sock in
  let ic = Lwt_io.(of_fd ~mode:Input clint_sock) in
  let oc = Lwt_io.(of_fd ~mode:Output clint_sock) in
  Lwt.async (fun () -> handle_message ic oc);
  server sock

let () =
  Lwt_main.run (let%lwt sock = create_socket () in
                server sock)
