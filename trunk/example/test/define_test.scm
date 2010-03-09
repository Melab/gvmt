

(define (while-expander while-exp)
    (define condition (cadr while-exp))
    (define body (cddr while-exp))
    (append `(do () ((not ,condition) #f)) body))

;(while-expander '(while (< 0 n) (set! n (- n 1))))
;
;(define-syntax while while-expander)
;
;(define n 10)
;(while (< 0 n) 
;    (set! n (- n 1))
;    (display n))
;
            
(list (make-vector 4) (make-vector 3 4))

