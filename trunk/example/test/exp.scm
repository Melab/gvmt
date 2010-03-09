(define (exp2 n)
    (if (= n 0)
        1
        (let* ((m (/ n 2))
                (x (exp2 m)))
            (if (= (- n m) m)
                (* x x)
                (* 2 (* x x))))))
                
(do ((i 0 (+ i 1)))
    ((> i 15))
    (display i)
    (display " ")
    (display (exp2 i))
    (newline))

