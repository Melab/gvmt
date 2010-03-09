        
(define (add1 n) (+ n 1))
(define (sub1 n) (- n 1))

(define (tail-count-aux n m)
    (if (= n 0)
        m
        (tail-count-aux (sub1 n) (add1 m))))

(define (tail-count n)
    (tail-count-aux n 0))

;(tail-count (string->number (vector-ref argv 1)))
            
;(tail-count 4)

;(tail-count-aux 2 0)

;(length '(1 2 3 4 5 6 7 8))
(display (abs 3))
(display (abs -3))
(display (abs 2))
(display (abs -3))
(reverse '())
(reverse '())
(display '())
(display '(1))
(display '(1 2))
(display (reverse '()))
(display (reverse '(1)))
(display (reverse '(1 2)))

;(exit)

