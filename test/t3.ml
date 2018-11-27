let i = ref 10
let a = Fiber.create (fun () -> while !i > 0 do
				  Printf.printf "a %i\n" !i;
				  decr i;
				  Fiber.yield ();
				done) ()
let b = Fiber.create (fun () -> while !i > 0 do
				  Printf.printf "b %i\n" !i;
				  decr i;
				  Fiber.resume a
				done) ()
let _ =
  Fiber.resume a;
  Fiber.resume b
