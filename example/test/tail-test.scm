(define (triangles n)
    (let loop ([t 0])
        (when (<= t n)
            (let loop2 ([x 0])
                (cond [(< x t)
                    (display x)
                    (loop2 (add1 x))]
                    [ else 
                    (newline)
                    (loop (add1 t))])))))
   
(triangles 8)

(define (flip n)
    (let loop ([a 0] [b 1] [c 2] [d n])
        (display (list a b c))
        (newline)
        (when (> d 0)
            (loop c b a (sub1 d)))))

(flip 3)
                
