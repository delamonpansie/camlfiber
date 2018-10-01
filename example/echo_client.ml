let count = ref 0

let message =
  "On an exceptionally hot evening early in July a young man came out of the garret in which he lodged in S. Place and walked slowly, as though in hesitation, towards K. bridge."

let rec io f ev sock buf ofs len =
  let open Fiber.Unix in
  match f sock buf ofs len with
  | 0 -> raise End_of_file
  | n when n < len ->
     Fiber.wait_io_ready sock ev;
     io f ev sock buf (ofs + n) (len - n)
  | exception Unix_error (EAGAIN, _, _) ->
     Fiber.wait_io_ready sock ev;
     io f ev sock buf ofs len
  | _ -> ()

let client (latency, port) =
  let open Fiber.Unix in
  let sock = socket PF_INET SOCK_STREAM 0 in
  connect sock (ADDR_INET (inet_addr_loopback, port)); (* TODO: add async wrapper *)
  setsockopt sock TCP_NODELAY true;
  set_nonblock sock;

  let wbuf = Bytes.of_string (" " ^ message) in
  let rbuf = Bytes.create (Bytes.length wbuf) in
  let loop () =
    while !count > 0 do
      decr count;
      let ix = !count in
      let mlen = 16 + Random.int 48 in
      Bytes.set wbuf 0 (Char.chr mlen);
      let t = Unix.gettimeofday () in
      io single_write EV_WRITE sock wbuf 0 mlen;
      io read EV_READ sock rbuf 0 mlen;
      (* assert (wbuf = rbuf); *)
      decr count;
      latency.(ix) <- (Unix.gettimeofday ()) -. t
    done;
    Fiber.break ()
  in
  Printexc.catch loop ()

let rec mon () =
  Printf.printf "%i\n%!" !count;
  Fiber.Unix.sleepf 0.3;
  mon ()

let _ =
  if Array.length Sys.argv < 4 then
    failwith ("usage: " ^ Sys.argv.(0) ^ " port n count");

  let port = int_of_string Sys.argv.(1) in
  let n = int_of_string Sys.argv.(2) in
  count := int_of_string Sys.argv.(3);

  let latency = Array.make !count 0.0 in

  Fiber.create mon () |> ignore;
  for _ = 1 to n do
    Fiber.create client (latency, port) |> ignore
  done;

  let t0 = Unix.gettimeofday () in
  Fiber.run ();

  Printf.printf "\n\n";

  let len = Array.length latency in
  let rps = float len /. (Unix.gettimeofday () -. t0) in
  Array.sort compare latency;
  let lat_min = latency.(0) in
  let lat_max = latency.(len - 1) in
  let lat_avg = Array.fold_left (+.) 0. latency /. float len in
  let lat_med = latency.(len / 2) in
  Printf.printf "RPS: %.1f\nlatency: \nmin %f\nmax %f\navg: %f\nmed: %f\n"
    rps lat_min lat_max lat_avg lat_med
