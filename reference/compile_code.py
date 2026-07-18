import os

# Output file name
output_file = "all_code.txt"

# Base directory to start scanning (current working directory)
base_dir = os.getcwd()

# File extensions to include (C++ related and original ones)
valid_extensions = {'.cpp', '.h', '.hpp', '.c', '.cc', '.cxx', 
                    '.java', 'readme.txt', 'cmakelists.txt'}  # Add more as needed

def process_directory(directory, outfile):
    """Recursively process all files in directory and its subdirectories."""
    try:
        for item in os.listdir(directory):
            filepath = os.path.join(directory, item)
            
            if os.path.isdir(filepath):
                # Recursively process subdirectories
                process_directory(filepath, outfile)
            
            elif os.path.isfile(filepath):
                # Check if file has a valid extension or is README.txt (case-insensitive)
                filename_lower = item.lower()
                if filename_lower.endswith(tuple(valid_extensions)):
                    outfile.write(f"// File: {filepath}\n\n")
                    try:
                        with open(filepath, "r", encoding="utf-8") as infile:
                            outfile.write(infile.read())
                    except Exception as e:
                        outfile.write(f"// Error reading file: {str(e)}\n")
                    outfile.write("\n\n")
    
    except PermissionError:
        outfile.write(f"// Note: Permission denied for {directory}\n\n")
    except Exception as e:
        outfile.write(f"// Note: Error processing {directory}: {str(e)}\n\n")

# Open the output file in write mode
with open(output_file, "w", encoding="utf-8") as outfile:
    # Process the base directory and all its subdirectories
    process_directory(base_dir, outfile)

print(f"Done! Check {output_file} for the compiled code.")
