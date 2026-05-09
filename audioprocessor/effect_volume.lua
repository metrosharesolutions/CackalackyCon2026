state = {
  volume = 1.0
}

local function ask_number(label, minValue, maxValue, defaultValue)
  io.write(label .. " (" .. minValue .. " to " .. maxValue .. ", default " .. defaultValue .. "): ")

  local input = io.read()

  if input == nil or input == "" then
    return defaultValue
  end

  local value = tonumber(input)

  if value == nil then
    print("Invalid number. Using default: " .. defaultValue)
    return defaultValue
  end

  if value < minValue then value = minValue end
  if value > maxValue then value = maxValue end

  return value
end

function init()
  print("Volume effect setup")
  state.volume = ask_number("Volume level", 0.0, 1.0, 1.0)
  print("Volume set to " .. state.volume)
end

function processFrame(left, right)
  local g = state.volume or 1.0

  left = tonumber(left) or 0.0
  right = tonumber(right) or 0.0

  return left * g, right * g
end