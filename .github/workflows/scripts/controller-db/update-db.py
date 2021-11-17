import os

relevant_categories = [
  "# Mac OS X",
  "# Linux"
]

header_lines = []
new_db_contents = []

def is_relevant_category(line):
  for category in relevant_categories:
    if category in line:
      return True
  return False

with open("./game_controller_db.txt") as file:
  lines = file.readlines()
  finished_header = False
  processing_section = False
  for line in lines:
    if finished_header is False:
      header_lines.append(line)
      if line == "\n":
        finished_header = True
    if processing_section and line == "\n":
      processing_section = False
      new_db_contents.append("\n")
    if is_relevant_category(line) and processing_section is False:
      processing_section = True
      new_db_contents.append(line)
    elif processing_section:
      new_db_contents.append(line)

os.remove("./game_controller_db.txt")
with open("./game_controller_db.txt", "w") as f:
  f.writelines(header_lines)
  f.writelines(new_db_contents)
