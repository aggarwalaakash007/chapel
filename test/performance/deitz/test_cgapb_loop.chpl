param N = 1024;
param ITERS = 200;

def main() {
  const D = [0..N-1, 0..N-1];
  var A, B, C: [D] int;
  [(i,j) in D] A(i,j) = i+j;
  [(i,j) in D] B(i,j) = i*j;
  for i in 1..ITERS do
    [(i,j) in D] C(i,j) = A(i,j) + B(i,j);
  writeln(C(51,51));
}
