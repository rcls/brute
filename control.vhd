library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity control is
  port (Clk_125MHz : in std_logic;
         LED : out std_logic_vector (7 downto 0));
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

  component Clock
  port (CLKIN_IN : in STD_LOGIC; CLKFX_OUT : out STD_LOGIC);
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
  subtype byte_t is std_logic_vector (7 downto 0);
  -- We use a 48 bit cycle counter.
  subtype clock_t is std_logic_vector (47 downto 0);
  subtype vector_144 is std_logic_vector (143 downto 0);
  
  component md5 is
    port (in0 : in word_t;
          in1 : in word_t;
          in2 : in word_t;
          
          hit : out std_logic;

          Aout : out word_t;
          Bout : out word_t;
          Cout : out word_t;
          Dout : out word_t;

          Clk : in std_logic);
  end component;

  signal Clk : std_logic;

  signal next0 : word_t;                -- 3 input words to md5.
  signal next1 : word_t;
  signal next2 : word_t;

  signal hit : std_logic;               -- Hit signal from md5.
  signal outA : word_t;                 -- 4 output words from md5.
  signal outB : word_t;
  signal outC : word_t;
  signal outD : word_t;

  -- 152 bit shift register attached to user1.  8 bit opcode plus 144 bits
  -- data.
  -- opcode 1 : read result + 8 bit address
  -- opcode 2 : load md5 - 48 bit clock count + 96 bits data.
  -- opcode 3 : sample md5 - 48 bit clock count
  -- opcode 4 : read clock count
  signal command : std_logic_vector (151 downto 0);
  alias command_opcode : byte_t is command (151 downto 144);
  alias command_clock : clock_t is command (143 downto 96);
  alias command_address : byte_t is command (143 downto 136);
  signal command_valid : std_logic := '0';
  -- Detect rising edge with "01" and falling by "10"; we're crossing
  -- clock domains.
  signal command_edge : std_logic_vector (1 downto 0) := "00";

  -- The dual ported hit ram.
  type hit_ram_t is array (255 downto 0) of vector_144;
  signal hit_ram : hit_ram_t;
  -- Output buffer the bram needs.
  signal hit_ram_o : vector_144;
  -- The hit ram allocation counter.
  signal hit_idx : byte_t;

  -- The 48 bit global cycle counter.
  signal global_count : clock_t;
  signal global_count_latch : clock_t;
  signal global_count_match : std_logic; -- Does global count match command?
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

   clock_inst : clock port map (
      CLKFX_OUT => Clk,
      CLKIN_IN => Clk_125MHz);

  -- The outputs; the jtag unit seems to take care of latching on falling TCK.
  jtag_tdo1 <= command (0);

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
          -- If the previous command was a read-result then grab the data out
          -- of the hit-ram.  Else grab the latched clock.
          if command_opcode = x"01" then
            command <= x"00" & hit_ram_o;
          else
            command <= x"00000000000000000000000000" & global_count_latch;
          end if;
        elsif jtag_update = '1' then
          command_valid <= '1';
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

  m : md5 port map (in0 => next0,
                    in1 => next1,
                    in2 => next2,

                    hit => hit,

                    Aout => outA,
                    Bout => outB,
                    Cout => outC,
                    Dout => outD,

                    Clk => Clk);

  global_count_match <= '1' when command_edge(1) = '1' and global_count = command_clock else '0';

  -- Calculate the next value to feed into the pipeline.
  process (outA, outB, outC, outD, command_opcode, global_count_match)
  begin
    if command_opcode = x"02" and global_count_match = '1' then
      next0 <= command (31 downto 0);
      next1 <= command (63 downto 32);
      next2 <= command (95 downto 64);
    else
      next0 <= outA;
      next1 <= outB;
      next2 <= outC;
    end if;
  end process;

  -- Write into the hit ram on hits, and on both commands 2 and 3.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      if hit = '1' or (command_opcode(7 downto 1) = "0000001"
                       and global_count_match = '1') then
        hit_ram (conv_integer (hit_idx)) <= global_count & next2 & next1 & next0;
        hit_idx <= hit_idx + 1;
      end if;
    end if;
  end process;

  -- Nice LEDs
  LED <= hit_idx;
  
  -- Read the hit_ram.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      hit_ram_o <= hit_ram (conv_integer (command_address));
    end if;
  end process;

  -- On command_valid rising, latch the global counter.
  process (Clk)
  begin
    if Clk'event and Clk = '1' and command_edge = "01" then
      global_count_latch <= global_count;
    end if;
  end process;

end Behavioral;
