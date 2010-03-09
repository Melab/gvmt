(define (zero? n) (eq? 0 n))
(define (even? n) (eq? (& -2 n) n))
(define (odd? n) (not (eq? (& -2 n) n)))
(define (caar l) (car (car l)))
(define (cadr l) (car (cdr l)))
(define (cddr l) (cdr (cdr l)))
(define (caddr l) (car (cdr (cdr l))))
(define (cadar l) (car (cdr (car l))))
(define (cdddr l) (cdr (cdr (cdr l))))
;(define first car)
;(define second cadr)
;(define third caddr)

(define (append l1 l2) 
    (if (null? l1)
        l2
        (cons (car l1) (append (cdr l1) l2))))

(define (max a b)
    (if (< a b)
        b
        a))

(define (newline) (display 1))
(define (newline) (display "\n"))

(define (print n)
    (display n)
    (newline)
    n)

(define (make-vector-expander size_maybe_default)
    (let ((maybe_default (cddr size_maybe_default))
          (size (cadr size_maybe_default)))
        (if (null? maybe_default)
            `(__make-vector ,size 0)
            `(__make-vector ,size ,(car maybe_default)))))

(define-syntax make-vector make-vector-expander)

(define (when-expander when-exp)
    (let ((condition (cadr when-exp))
          (rest (cddr when-exp)))
        `(if ,condition ,(cons 'begin rest) #void)))

(define-syntax when when-expander)

;(define (vector-copy-expander vc-exp)
;    (let ((dest (cadr vc-exp))
;          (dest-start (caddr vc-exp))
;          (rest (cdddr vc-exp)))
;        (let ((src (car rest))
;              (start-end (cdr rest)))
;            (if (null? start-end)
;                `(__vector-copy! ,dest ,dest-start ,src 0 (vector-length ,src))
;                `(__vector-copy! ,dest ,dest-start ,src ,(car start-end),(cadr start-end))))))

;(define-syntax vector-copy! vector-copy-expander)

(define (__cond-clause-expander clauses)
    (if (null? clauses)
        '#void
        (let ((test-expr (car clauses))
          (rest (cdr clauses)))
            (let ((test (car test-expr))
                  (exprs (cdr test-expr)))
                (if (equal? test 'else)
                    (cons 'begin exprs)
                    `(if ,test ,(cons 'begin exprs) ,(__cond-clause-expander rest)))))))
                    

(define (cond-expander cond-exp)
    (__cond-clause-expander (cdr cond-exp)))
          
(define-syntax cond  cond-expander)         
          
(define (abs a) 
    (if (< a 0) (- 0 a) a))

(define (__reverse-helper reversed l)
    (if (null? l)
        reversed
        (__reverse-helper (cons (car l) reversed) (cdr l))))
          
(define (reverse l)
    (__reverse-helper '() l))

(define (length l)
    (if (null? l)
        0
        (+ 1 (length (cdr l)))))

;Implement so that can use same source file for gvmt-scheme & bigloo
(define (module name)
    #void)
 
(define (add1 n) (+ n 1))
(define (sub1 n) (- n 1))

