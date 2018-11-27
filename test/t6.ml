let main () =
  let rec loop () =
    print_string "loop: ";
    Fiber.yield ();
    loop () in
  let l = Fiber.create loop () in
  for i = 1 to 10 do
    Fiber.resume l;
    print_int i;
    print_newline ();
  done

let _ = Fiber.run main ()
