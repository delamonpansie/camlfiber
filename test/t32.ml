
let rec consumer () =
  match Fiber.unsafe_yield () with
    Some i -> print_int i;
	      print_newline ();
	      consumer ()
  | None -> ()

let producer (fb, n) =
  for i = 1 to n do
    print_int i;
    print_string " -> ";
    Fiber.unsafe_resume fb (Some i)
  done

let _ =
  let fb = Fiber.create consumer () in
  Fiber.resume fb; (* after resume [fb] will be suspended on unsafe_yield *)
  Fiber.create producer (fb, 5) |> Fiber.resume
