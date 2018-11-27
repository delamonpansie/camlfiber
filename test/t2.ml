[@@@warning "-52"]

let _=
  let f = Fiber.create print_string "hello, world\n" in
  Fiber.resume f;
  let print_exc e = print_string (Printexc.to_string e);
                    print_newline () in
  (* yield must be called only from inside of fiber *)
  (try Fiber.yield ();
   with Invalid_argument("Fiber.yield") as e -> print_exc e);
  (try Fiber.unsafe_yield () |> ignore;
   with Invalid_argument("Fiber.unsafe_yield") as e -> print_exc e);
  (* f is dead here, wake and resumy should raise an exception *)
  (try Fiber.resume f
   with Invalid_argument("Fiber.resume") as e -> print_exc e);
  (try Fiber.unsafe_resume f ()
   with Invalid_argument("Fiber.unsafe_resume") as e -> print_exc e);
  (try Fiber.wake f;
   with Invalid_argument("Fiber.wake") as e -> print_exc e);
  (try Fiber.cancel_wake f;
   with Invalid_argument("Fiber.cancel_wake") as e -> print_exc e);
