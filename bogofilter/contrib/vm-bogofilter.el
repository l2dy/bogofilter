;;; vm-bogofilter.el version 1.0
;;
;; An interface between the VM mail reader and the bogofilter spam filter.
;;
;; Copyright (C) 2003 by Bjorn Knutsson
;;
;; Home page: http://www.cis.upenn.edu/~bjornk/
;;
;; Bjorn Knutsson, CIS, 3330 Walnut Street, Philadelphia, PA 19104-6389, USA
;;
;;
;; Based on vm-spamassassin.el v1.1, Copyright (C) 2002 by Markus Mohnen
;;
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;;
;;; Version history:
;;
;; v 1.0: initial release
;;        * First release, based on Markus Mohnen's vm-spamassassin
;;
;;
;; To use this program, you need reasonably recent versions of VM from
;; http://www.wonderworks.com/vm) and bogofilter from
;; http://sourceforge.net/projects/bogofilter/
;;
;; This interface was developed for VM 7.15 and bogofilter 0.13.7.1
;;
;;; Installation:
;;
;;  Put this file on your Emacs-Lisp load path and add following into your
;;  ~/.vm startup file
;;
;;      (require 'vm-bogofilter)
;;
;;
;;; Usage:
;;
;; Whenever you get new mail bogofilter will be invoked on them. Mail
;; detected as spam will be tagged by bogofilter, and you can use
;; existing mechanisms to dispose of them.
;;
;; For example, if you append this line to your .vm (or modify your
;; existing auto-folder-alist), you could then have messages tagged as
;; spam automatically saved in a separate 'spam' folder:
;;
;; (setq vm-auto-folder-alist '(("^X-Bogosity: " ("Yes," . "spam"))))
;;
;; If a message is tagged as spam incorrectly, you can re-train
;; bogofilter by calling the function vm-bogofilter-is-clean on that
;; message. Similarly, calling vm-bogofilter-is-spam will re-train
;; bogofilter to recognize a clean-marked message as spam.
;;
;; These functions can be bound to keysin your .vm, for example:
;;
;; (define-key vm-mode-map "K" 'vm-bogofilter-is-spam)
;; (define-key vm-mode-map "C" 'vm-bogofilter-is-clean)
;;
;; would define K (shift-k) as the key to declare the current message
;; as spam, while C (shift-c) as the key to declare the current
;; message as clean.
;;
;; NOTE!! The current implementation of clean/spam will *not* change
;; the tag inside the message, only the content of bogofilter's
;; database.
;;
;; This means that if you take actions based on the headers of the
;; mail, these actions will be triggered. (However, if you are using
;; auto-folders, it will not try to move a message already marked as
;; filed.)
;;
;; Because of the way bogofilter works, even a message explicitly
;; declared as spam may not be tagged as spam if there are enough
;; similar non-spam messages. Remember, bogofilter is not trained to
;; recognize individual messages, but rather patterns. You may have to
;; train bogofilter on a number of spam messages before it recognizes
;; any of them as spam. See the documentation for bogofilter.
;;
;;; BUGS:
;;
;; One know bug is that formail will not like it if the input is not
;; in the format it expects and knows. Even though it's supposed to
;; know BABYL, this does not work.
;;
;;
;;; Customization:
;;
;;  M-x customize RET vm-bogofilter

;;; Code:

;;; Customisation:

(defgroup vm-bogofilter nil
  "VM Spam Filter Options"
  :group 'vm)

(defcustom vm-bogofilter-program "bogofilter"
  "*Name of the bogofilter program."
  :group 'vm-bogofilter
  :type 'string)

(defcustom vm-bogofilter-program-options "-u -p -e"
  "*Options for the bogofilter program. Since we use bogofilter as a filter, '-p'
must be one of the options."
  :group 'vm-bogofilter
  :type 'string)

(defcustom vm-bogofilter-program-options-unspam "-Sn"
  "*Options for the bogofilter program when declaring a spam-marked message as clean."
  :group 'vm-bogofilter
  :type 'string)

(defcustom vm-bogofilter-program-options-spam "-Ns"
  "*Options for the bogofilter program when declaring a clean-marked message as spam."
  :group 'vm-bogofilter
  :type 'string)

(defcustom vm-bogofilter-formail-program "formail"
  "*Name of the program used to split a sequence of mails."
  :group 'vm-bogofilter
  :type 'string)

(defcustom vm-bogofilter-formail-program-options "-s"
  "*Options for the 'vm-bogofilter-formail-program'. After this arguments, the name
of the bogofilter program will be passed." 
  :group 'vm-bogofilter
  :type 'string)

(defun vm-bogofilter-arrived-message ()
  "The function used to do the actual filtering. It is used as a value for
vm-retrieved-spooled-mail-hook."
  (save-excursion
    (vm-save-restriction
     (let ((tail-cons (vm-last vm-message-list))
	   (buffer-read-only nil))
       (widen)
       (if (null tail-cons)
	   (goto-char (point-min))
	 (goto-char (vm-text-end-of (car tail-cons)))
	 (forward-line)
	 )
       (message "Filtering new mails... ")
       (call-process-region (point) (point-max)
			    (or shell-file-name "sh")
			    t t nil shell-command-switch
			    (concat vm-bogofilter-formail-program " "
				    vm-bogofilter-formail-program-options " "
				    vm-bogofilter-program " "
				    vm-bogofilter-program-options))
       (message "Assassinating new mails... done")
       )
     )
    )
  )

(defun vm-bogofilter-is-spam ()
  "Declare that a clean-marked message is spam"
  (interactive)
  (vm-pipe-message-to-command
   (concat vm-bogofilter-program " " vm-bogofilter-program-options-spam) nil)
  )

(defun vm-bogofilter-is-clean ()
  "Declare that a spam-marked message is clean"
  (interactive)
  (vm-pipe-message-to-command
   (concat vm-bogofilter-program " " vm-bogofilter-program-options-unspam) nil)
  )

 ;;; Hooking into VM

 (add-hook 'vm-retrieved-spooled-mail-hook 'vm-bogofilter-arrived-message)

 (provide 'vm-bogofilter)

 ;;; vm-bogofilter.el ends here
