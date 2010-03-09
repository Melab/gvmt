;(define (sub1 n) (- n 1))

(let fac ((n 5))
      (if (zero? n)
          1
          (* n (fac (- n 1)))))
;
;((lambda ()
;    (define add (lambda (a b) (+ a b)))
;    (add 2 3)))
;
;((lambda ()
;    (define fac (lambda (n)
;            (if (zero? n)
;            1
;            (* n (fac (sub1 n))))))
;    (fac 3)))
;
;((lambda ()
;    (define sum (lambda (n)
;            (if (zero? n)
;            0
;            (+ n (sum (- n 1))))))
;    (sum 1)))

;((lambda ()
;    (define x (lambda () 5))
;    (x)))
;
;(fac 5)

