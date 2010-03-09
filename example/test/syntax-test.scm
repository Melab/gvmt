;(define n 5)
;
;(let* () n)
;
;(let* ((min-depth 4)
;         (max-depth (max (+ min-depth 2) n)))
;       (let ((stretch-depth (+ max-depth 1)))
;            (display stretch-depth)))

(define u-test '(let ((a 1) (b 2)) ( + a b)))
(define n-test '(let x ((a 1) (b 2)) ( + a b)))

(display (unnamed-let-expander u-test)) (newline)
(display (named-let-expander n-test)) (newline)
(display (let-expander u-test)) (newline)
(display (let-expander n-test)) (newline)

