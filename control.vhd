library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity control is
  port (Clk : std_logic);
  attribute loc : string;
  attribute loc of Clk : signal is "P26";
end control;

architecture Behavioral of control is

  component BSCAN_SPARTAN3A
    port (TCK : out STD_ULOGIC;
          TMS : out STD_ULOGIC;
          CAPTURE : out STD_ULOGIC;
          DRCK1 : out STD_ULOGIC;
          DRCK2 : out STD_ULOGIC;
          RESET : out STD_ULOGIC;
          SEL1 : out STD_ULOGIC;
          SEL2 : out STD_ULOGIC;
          SHIFT : out STD_ULOGIC;
          TDI : out STD_ULOGIC;
          UPDATE : out STD_ULOGIC;
          TDO1 : in STD_ULOGIC;
          TDO2 : in STD_ULOGIC);
  end component; 

  signal jtag_tck : std_logic;
  signal jtag_tms : std_logic;
  signal jtag_capture : std_logic;
  signal jtag_drck1 : std_logic;
  signal jtag_drck2 : std_logic;
  signal jtag_sel1 : std_logic;
  signal jtag_sel2 : std_logic;
  signal jtag_shift : std_logic;
  signal jtag_tdi : std_logic;
  signal jtag_update : std_logic;
  signal jtag_tdo1 : std_logic;
  signal jtag_tdo2 : std_logic;

  subtype word_t is std_logic_vector (31 downto 0);

  component feeder is
    port (load : in std_logic;
          load0 : in word_t;
          load1 : in word_t;
          load2 : in word_t;
          
          hit : out std_logic;

          hitA : out word_t;
          hitB : out word_t;
          hitC : out word_t;
          hitD : out word_t;
          
          Clk : in std_logic);
  end component;

  signal f_load : std_logic;
  signal f_load0 : word_t;
  signal f_load1 : word_t;
  signal f_load2 : word_t;

  signal f_hit : std_logic;
  signal f_hitA : word_t;
  signal f_hitB : word_t;
  signal f_hitC : word_t;
  signal f_hitD : word_t;
  
  subtype dat144_t is std_logic_vector (143 downto 0);

  -- 152 bit shift register attached to user1.  8 bit opcode plus 144 bits
  -- data.  Input from jtag to us only.
  -- opcode 1 : load md5 - 48 bit clock count + 96 bits data.
  -- opcode 2 : sample md5 - 48 bit clock count
  -- opcode 3 : read result + 8 bit address
  -- opcode 4 : read clock count
  signal command : std_logic_vector (151 downto 0);
  signal command_valid : std_logic := '0';
  -- Detect rising edge with "01" and falling by "10"; we're crossing
  -- clock domains.
  signal command_edge : std_logic_vector (1 downto 0) := "00";
  
  -- 144 bit shift register attached to user2.  Result data.  Output from us
  -- to jtag only.
  signal result_sr : dat144_t;

  -- The result loaded into result_sr in the appropriate capture_dr state.
  signal result : dat144_t;

  -- The dual ported hit ram.
  type hit_ram_t is array (255 downto 0) of dat144_t;
  signal hit_ram : hit_ram_t;
  -- Output buffer the bram needs.
  signal hit_ram_o : dat144_t;
  -- The hit ram allocation counter.
  signal hit_idx : std_logic_vector (7 downto 0);

  -- The 48 bit global cycle counter.
  signal global_count : std_logic_vector (47 downto 0);
  signal global_count_latch : std_logic_vector (47 downto 0);
begin

  BSCAN_SPARTAN3_inst : BSCAN_SPARTAN3A
    port map (
      TCK => jtag_tck,
      TMS => jtag_tms,
      CAPTURE => jtag_capture, -- CAPTURE output from TAP controller
      DRCK1 => jtag_drck1,     -- Data register output for USER1 functions
      DRCK2 => jtag_drck2,        -- Data register output for USER2 functions
      --RESET => jtag_RESET,        -- Reset output from TAP controller
      SEL1 => jtag_sel1,          -- USER1 active output
      SEL2 => jtag_sel2,          -- USER2 active output
      SHIFT => jtag_shift,        -- SHIFT output from TAP controller
      TDI => jtag_tdi,            -- TDI output from TAP controller
      UPDATE => jtag_update,      -- UPDATE output from TAP controller
      TDO1 => jtag_tdo1,          -- Data input for USER1 function
      TDO2 => jtag_tdo2           -- Data input for USER2 function
      );

  -- The outputs; the jtag unit seems to take care of latching on falling TCK.
  jtag_tdo1 <= command (0);
  jtag_tdo2 <= result_sr (0);

  process (jtag_tck)
  begin
    if jtag_tck'event and jtag_tck = '1' then
      if jtag_sel1 = '1' then
        if jtag_shift = '1' then
          -- Shift in the command.
          command <= jtag_tdi & command (151 downto 1);
        end if;
        if jtag_capture = '1' then
          command_valid <= '0';
        elsif jtag_update = '1' then
          command_valid <= '1';
        end if;
      end if;

      if jtag_sel2 = '1' then
        if jtag_capture = '1' then
          result_sr <= result;
        elsif jtag_shift = '1' then
          result_sr <= jtag_tdi & result_sr (143 downto 1);
        end if;
      end if;
    end if;
  end process;

  -- Run the global counter & command_valid edge detection.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      global_count <= global_count + x"000000000001";
      command_edge(0) <= command_valid;
      command_edge(1) <= command_edge(0);
    end if;
  end process;

  f : feeder port map (
    load => f_load,
    load0 => f_load0,
    load1 => f_load1,
    load2 => f_load2,

    hit => f_hit,

    hitA => f_hitA,
    hitB => f_hitB,
    hitC => f_hitC,
    hitD => f_hitD,

    Clk => Clk
    );

  -- Write into the hit ram.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      if f_hit = '1' or (command_edge (1) = '1' and
                         command(151 downto 96) = x"02" & global_count) then
        hit_ram (conv_integer (hit_idx)) <= global_count & f_hitC & f_hitB & f_hitA;
        hit_idx <= hit_idx + 1;
      end if;
    end if;
  end process;

  -- Read the hit_ram.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      hit_ram_o <= hit_ram (conv_integer (command (143 downto 136)));
    end if;
  end process;

  -- On command_valid rising, latch the global counter.
  process (Clk)
  begin
    if Clk'event and Clk = '1' and command_edge = "01" then
      global_count_latch <= global_count;
    end if;
  end process;

  -- Load into the pipeline.
  f_load <= '1' when command(151 downto 96) = x"01" & global_count else '0';
  f_load0 <= command (31 downto 0);
  f_load1 <= command (63 downto 32);
  f_load2 <= command (95 downto 64);
  
  -- Select the result.
  process (command, hit_ram_o, global_count_latch)
  begin
    case command (151 downto 144) is
      when x"03" => result <= hit_ram_o;
      when x"04" => result <= global_count_latch & x"000000000000000000000000";
      when others => result <= x"000000000000000000000000000000000000";
    end case;
  end process;
end Behavioral;
