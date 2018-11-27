let main () =
  let open Unix in
  let a, b = socketpair PF_UNIX SOCK_SEQPACKET 0 in
  set_nonblock a;
  set_nonblock b;
  let n = 64 in
  let reader = Fiber.create (fun () ->
      let buf = Bytes.create 256 in
      for i = 1 to n do
	Fiber.wait_io_ready a Fiber.EV_READ;
	let n = read a buf 0 256 in
	print_string "read:  ";
	print_string (Bytes.sub_string buf 0 n);
	print_newline ()
      done) ()
  in
  let writer = Fiber.create (fun () ->
      let buf = Bytes.make 256 'a' in
      for i = 1 to n do
	Fiber.wait_io_ready b Fiber.EV_WRITE;
	let n' = single_write b buf 0 i in
	print_string "write: ";
	print_string (Bytes.sub_string buf 0 n');
	print_newline ()
      done) () in
  Fiber.resume reader;
  Fiber.resume writer;
  Fiber.join reader

let _ = Fiber.run main ()
