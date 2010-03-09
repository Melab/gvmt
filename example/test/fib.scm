        
(define (fib x)
    (if (<= x 1)
        1
        (+ (fib (- x 2)) (fib (- x 1)))))

(display (fib 33.0))
(newline)

