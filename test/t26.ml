let main () =
  let q = Queue.create () in
  for i = 1 to 50 do
    let f = Fiber.create (fun () ->
		Fiber.sleep 0.02;
		print_int i;
		print_newline ()) () in
    Fiber.resume f;
    Queue.push f q;
    Fiber.sleep 0.01;
  done;
  while not (Queue.is_empty q) do
    Fiber.join (Queue.pop q)
  done

let _ = Fiber.run main ()
