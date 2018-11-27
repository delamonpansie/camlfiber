
let _ =
  let fiber = Fiber.create (fun _ -> while true do Fiber.yield () done) () in
  Benchmark.throughput1 ~repeat:10 ~name:"resume" 3 Fiber.resume fiber
  |> Benchmark.tabulate
