let main () =
  let c = Fiber.Condition.create () in
  let rec loop () =
    print_string "loop: ";
    Fiber.Condition.signal c;
    Fiber.yield ();
    loop () in
  let l = Fiber.create loop () in
  for i = 1 to 10 do
    Fiber.wake l;
    Fiber.Condition.wait c;
    print_int i;
    print_newline ();
  done

let _ = Fiber.run main ()
