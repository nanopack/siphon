require 'open3'

STDOUT.sync = STDERR.sync = true # don't buffer stdout/stderr

# cmd = "/Users/tylerflint/Work/nanopack/unbuffer/src/unbuffer npm install -g express | /Users/tylerflint/Work/nanopack/siphon/src/siphon --prefix ''"
# cmd = "npm install -g express | /Users/tylerflint/Work/nanopack/siphon/src/siphon --prefix ''"
# cmd = "unbuffer npm install -g express | ../src/siphon --prefix '-> '"
# cmd = "unbuffer docker pull nanobox/portal | /Users/tylerflint/Work/nanopack/siphon/src/siphon --prefix '-> '"
# cmd = "unbuffer stty size"
cmd = "../src/siphon --prefix=\"-> \" -- npm install -g express"

Open3.popen3 cmd do |stdin, stdout, stderr, wait_thr|
  stdout_eof = false
  stderr_eof = false

  until stdout_eof and stderr_eof do
    (ready_pipes, dummy, dummy) = IO.select([stdout, stderr])
    ready_pipes.each_with_index do |socket|
      if socket == stdout
        begin
          print socket.readpartial(1)
          # puts "out -> #{socket.readpartial(4096)}"
        rescue EOFError
          stdout_eof = true
        end
      elsif socket == stderr
        begin
          print socket.readpartial(1)
          # puts "err -> #{socket.readpartial(4096)}"
        rescue EOFError
          stderr_eof = true
        end
      end
    end
  end

  exit_status = wait_thr.value.to_s.match(/exit (\d+)/)[1].to_i

  # puts "exit: #{exit_status}"
end
