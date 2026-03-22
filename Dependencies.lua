function download_progress(total, current)
    local ratio = current / total;
    ratio = math.min(math.max(ratio, 0), 1);
    local percent = math.floor(ratio * 100);
    print("Download progress (" .. percent .. "%/100%)")
end

function check_raylib()
    print("Checking for Raylib...")
    os.chdir("Vendor")
    if(os.isdir("Sources") == false) then
        os.mkdir("Sources")
    end
    os.chdir("Sources")
    if(os.isdir("raylib-master") == false) then
        if(not os.isfile("raylib-master.zip")) then
            print("Raylib not found, downloading from https://github.com/raysan5/raylib/archive/refs/heads/master.zip")
            local result_str, response_code = http.download("https://github.com/raysan5/raylib/archive/refs/heads/master.zip", "raylib-master.zip", {
                progress = download_progress,
                headers = { "From: Premake", "Referer: Premake" }
            })
        end
        print("Unzipping to " ..  os.getcwd())
        zip.extract("raylib-master.zip", os.getcwd())
        os.remove("raylib-master.zip")
    end
    os.chdir("../../")
end

function check_raygui()
    print("Checking for Raygui...")
    os.chdir("Vendor")
    if(os.isdir("Sources") == false) then
        os.mkdir("Sources")
    end
    os.chdir("Sources")
    if(os.isdir("raygui-master") == false) then
        if(not os.isfile("raygui-master.zip")) then
            print("Raylib not found, downloading from https://github.com/raysan5/raygui/archive/refs/heads/master.zip")
            local result_str, response_code = http.download("https://github.com/raysan5/raygui/archive/refs/heads/master.zip", "raygui-master.zip", {
                progress = download_progress,
                headers = { "From: Premake", "Referer: Premake" }
            })
        end
        print("Unzipping to " ..  os.getcwd())
        zip.extract("raygui-master.zip", os.getcwd())
        os.remove("raygui-master.zip")
    end
    os.chdir("../../")
end

function check_glm()
    print("Checking for GLM...")
    print(os.getcwd())
    os.chdir("Vendor")
    if(os.isdir("Sources") == false) then
        os.mkdir("Sources")
    end
    os.chdir("Sources")
    if(os.isdir("glm-master") == false) then
        if(not os.isfile("glm-master.zip")) then
            print("GLM not found, downloading from https://github.com/g-truc/glm/archive/refs/heads/master.zip")
            local result_str, response_code = http.download("https://github.com/g-truc/glm/archive/refs/heads/master.zip", "glm-master.zip", {
                progress = download_progress,
                headers = { "From: Premake", "Referer: Premake" }
            })
        end
        print("Unzipping to " ..  os.getcwd())
        zip.extract("glm-master.zip", os.getcwd())
        os.remove("glm-master.zip")
    end
    os.chdir("../../")
end

function check_nlohmann()
    print("Checking for nlohmann json...")
    print(os.getcwd())
    os.chdir("Vendor")
    if(os.isdir("Sources") == false) then
        os.mkdir("Sources")
    end
    os.chdir("Sources")
    if(os.isdir("nlohmann") == false) then
        os.mkdir("nlohmann")
        os.chdir("nlohmann")
        if(not os.isfile("json.hpp")) then
            print("nlohmann json not found, downloading from https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp")
            local result_str, response_code = http.download("https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp", "json.hpp", {
                progress = download_progress,
                headers = { "From: Premake", "Referer: Premake" }
            })
        end
    end
    os.chdir("../../../")
end

function build_externals()
     print("Checking external dependencies...")
     check_raylib()
end

build_externals()